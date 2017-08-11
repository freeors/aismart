/* $Id: font.cpp 47191 2010-10-24 18:10:29Z mordante $ */
/* vim:set encoding=utf-8: */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "global.hpp"

#include "config.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "rose_config.hpp"
#include "marked-up_text.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include "serialization/string_utils.hpp"
#include "help.hpp"
#include "gui/widgets/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "wml_exception.hpp"
#include "image.hpp"
#include "integrate.hpp"
#include "gettext.hpp"

#include <boost/foreach.hpp>
#include <list>
#include <set>
#include <stack>

// Signed int. Negative values mean "no subset".
typedef int subset_id;

struct font_id
{
	font_id(subset_id subset, int size) : subset(subset), size(size) {};
	bool operator==(const font_id& o) const
	{
		return subset == o.subset && size == o.size;
	};
	bool operator<(const font_id& o) const
	{
		return subset < o.subset || (subset == o.subset && size < o.size);
	};

	subset_id subset;
	int size;
};

static std::map<font_id, TTF_Font*> font_table;
static std::vector<std::string> font_names;

struct text_chunk
{
	text_chunk(subset_id subset) :
		subset(subset),
		text()
	{
	}

	bool operator==(text_chunk const & t) const { return subset == t.subset && text == t.text; }
	bool operator!=(text_chunk const & t) const { return !operator==(t); }

	subset_id subset;
	std::string text;
};

struct char_block_map
{
	char_block_map()
		: cbmap()
	{
	}

	typedef std::pair<int, subset_id> block_t;
	typedef std::map<int, block_t> cbmap_t;
	cbmap_t cbmap;
	/** Associates not-associated parts of a range with a new font. */
	void insert(int first, int last, subset_id id)
	{
		if (first > last) return;
		cbmap_t::iterator i = cbmap.lower_bound(first);
		// At this point, either first <= i->first or i is past the end.
		if (i != cbmap.begin()) {
			cbmap_t::iterator j = i;
			--j;
			if (first <= j->second.first /* prev.last */) {
				insert(j->second.first + 1, last, id);
				return;
			}
		}
		if (i != cbmap.end()) {
			if (/* next.first */ i->first <= last) {
				insert(first, i->first - 1, id);
				return;
			}
		}
		cbmap.insert(std::make_pair(first, block_t(last, id)));
	}
	/**
	 * Compresses map by merging consecutive ranges with the same font, even
	 * if there is some unassociated ranges inbetween.
	 */
	void compress()
	{
		cbmap_t::iterator i = cbmap.begin(), e = cbmap.end();
		while (i != e) {
			cbmap_t::iterator j = i;
			++j;
			if (j == e || i->second.second != j->second.second) {
				i = j;
				continue;
			}
			i->second.first = j->second.first;
			cbmap.erase(j);
		}
	}
	subset_id get_id(int ch)
	{
		cbmap_t::iterator i = cbmap.upper_bound(ch);
		// At this point, either ch < i->first or i is past the end.
		if (i != cbmap.begin()) {
			--i;
			if (ch <= i->second.first /* prev.last */)
				return i->second.second;
		}
		return -1;
	}
};

static char_block_map char_blocks;

//cache sizes of small text
typedef std::map<std::string,SDL_Rect> line_size_cache_map;

//map of styles -> sizes -> cache
static std::map<int,std::map<int,line_size_cache_map> > line_size_cache;

#define is_unprintable_wchar(ch)	((ch) < 0x20 && (ch) != '\t')
#define is_unprintable_uchar(ch)	((ch) < 0x20 && (ch) != '\t')

//Splits the UTF-8 text into text_chunks using the same font.
static std::vector<text_chunk> split_text(std::string const & utf8_text) 
{
	text_chunk current_chunk(0);
	std::vector<text_chunk> chunks;

	if (utf8_text.empty())
		return chunks;

	try {
		utils::utf8_iterator ch(utf8_text);
		int sub = char_blocks.get_id(*ch);
		if (sub >= 0) {
			current_chunk.subset = sub;
		}
		for (utils::utf8_iterator end = utils::utf8_iterator::end(utf8_text); ch != end; ++ch) {
			wchar_t wch = *ch;
			if (is_unprintable_wchar(wch)) {
				// as if control-asii, TTF_RenderUTF8_Blended will create little-width white surface.
				// git rid of these characters.	
				continue;
			}
			sub = char_blocks.get_id(wch);
			if (sub >= 0 && sub != current_chunk.subset) {
				chunks.push_back(current_chunk);
				current_chunk.text.clear();
				current_chunk.subset = sub;
			}
			current_chunk.text.append(ch.substr().first, ch.substr().second);
		}
		if (!current_chunk.text.empty()) {
			chunks.push_back(current_chunk);
		}
	}
	catch (utils::invalid_utf8_exception&) {
		// Invalid UTF-8 string: $utf8_text
	}
	return chunks;
}

static TTF_Font* open_font(const std::string& fname, int size)
{
	std::string name;
#ifdef ANDROID
	name = game_config::preferences_dir + "/fonts/" + fname;
#else
	if(!game_config::path.empty()) {
		name = game_config::path + "/fonts/" + fname;
		if(!file_exists(name)) {
			name = "fonts/" + fname;
			if(!file_exists(name)) {
				name = fname;
				if(!file_exists(name)) {
					// Failed opening font: $name: No such file or directory
					return NULL;
				}
			}
		}

	} else {
		name = "fonts/" + fname;
		if (!file_exists(name)) {
			if(!file_exists(fname)) {
				// Failed opening font: $name: No such file or directory
				return NULL;
			}
			name = fname;
		}
	}
#endif

	TTF_Font* font = TTF_OpenFont(name.c_str(),size);
	if(font == NULL) {
		// Failed opening font: TTF_OpenFont: $TTF_GetError()
		return NULL;
	}

	return font;
}

static TTF_Font* get_font(font_id id)
{
	const std::map<font_id, TTF_Font*>::iterator it = font_table.find(id);
	if(it != font_table.end())
		return it->second;

	if(id.subset < 0 || size_t(id.subset) >= font_names.size())
		return NULL;

	TTF_Font* font = open_font(font_names[id.subset], id.size);

	if(font == NULL)
		return NULL;

	TTF_SetFontStyle(font,TTF_STYLE_NORMAL);

	font_table.insert(std::pair<font_id,TTF_Font*>(id, font));
	return font;
}

static void clear_fonts()
{
	for(std::map<font_id,TTF_Font*>::iterator i = font_table.begin(); i != font_table.end(); ++i) {
		TTF_CloseFont(i->second);
	}

	font_table.clear();
	font_names.clear();
	char_blocks.cbmap.clear();
	line_size_cache.clear();
}

struct font_style_setter
{
	font_style_setter(TTF_Font* font, int style)
		: font_(font)
		, old_style_(0)
	{
		if (style == 0) {
			style = TTF_STYLE_NORMAL;
		}

		old_style_ = TTF_GetFontStyle(font_);
		TTF_SetFontStyle(font_, style);
	}

	~font_style_setter()
	{
		TTF_SetFontStyle(font_,old_style_);
	}

private:
	TTF_Font* font_;
	int old_style_;
};

namespace font {

manager::manager()
{
	const int res = TTF_Init();
	if(res == -1) {
		// Could not initialize true type fonts
		throw error();
	} else {
		// Initialized true type fonts
	}

	init();
}

manager::~manager()
{
	deinit();

	clear_fonts();
	TTF_Quit();
}

void manager::update_font_path() const
{
	deinit();
	init();
}

void manager::init() const
{
}

void manager::deinit() const
{
}

//structure used to describe a font, and the subset of the Unicode character
//set it covers.
struct subset_descriptor
{
	subset_descriptor() :
		name(),
		present_codepoints()
	{
	}

	std::string name;
	typedef std::pair<int, int> range;
	std::vector<range> present_codepoints;
};

//sets the font list to be used.
static void set_font_list(const std::vector<subset_descriptor>& fontlist)
{
	clear_fonts();

	std::vector<subset_descriptor>::const_iterator itor;
	for(itor = fontlist.begin(); itor != fontlist.end(); ++itor) {
		// Insert fonts only if the font file exists
		if(game_config::path.empty() == false) {
			if(!file_exists(game_config::path + "/fonts/" + itor->name)) {
				if(!file_exists("fonts/" + itor->name)) {
					if(!file_exists(itor->name)) {
					// Failed opening font file '$itor->name': No such file or directory
					continue;
					}
				}
			}
		} else {
			if(!file_exists("fonts/" + itor->name)) {
				if(!file_exists(itor->name)) {
					// Failed opening font file '$itor->name': No such file or directory
					continue;
				}
			}
		}
		const subset_id subset = font_names.size();
		font_names.push_back(itor->name);

		BOOST_FOREACH (const subset_descriptor::range &cp_range, itor->present_codepoints) {
			char_blocks.insert(cp_range.first, cp_range.second, subset);
		}
	}
	char_blocks.compress();
}

const SDL_Color NORMAL_COLOR = {0xDD,0xDD,0xDD,0xff},
                GRAY_COLOR   = {0x77,0x77,0x77,0xff},
                LOBBY_COLOR  = {0xBB,0xBB,0xBB,0xff},
                GOOD_COLOR   = {0x00,0xFF,0x00,0xff},
                BAD_COLOR    = {0xFF,0x00,0x00,0xff},
                BLACK_COLOR  = {0x00,0x00,0x00,0xff},
                YELLOW_COLOR = {0xFF,0xFF,0x00,0xff},
                BUTTON_COLOR = {0xBC,0xB0,0x88,0xff},
                PETRIFIED_COLOR = {0xA0,0xA0,0xA0,0xff},
                TITLE_COLOR  = {0xBC,0xB0,0x88,0xff},
				LABEL_COLOR  = {0x6B,0x8C,0xFF,0xff},
				BLUE_COLOR   = {0x00,0x00,0xFF,0xff}, 
				BIGMAP_COLOR = {0xFF,0xFF,0xFF,0xff};
const SDL_Color DISABLED_COLOR = inverse(PETRIFIED_COLOR);

int SIZE_SMALLEST = 0;
int SIZE_SMALLER = 0;
int SIZE_SMALL = 0;
int SIZE_DEFAULT = 0;
int SIZE_LARGE = 0;
int SIZE_LARGER = 0;
int SIZE_LARGEST = 0;
}

static const size_t max_text_line_width = 8192;

class text_surface
{
public:
	text_surface(std::string const &str, int size, SDL_Color color, int style);

	void measure() const;
	size_t width() const;
	size_t height() const;
	std::vector<surface> const & get_surfaces() const;

	bool operator==(text_surface const &t) const {
		return hash_ == t.hash_ && font_size_ == t.font_size_
			&& color_ == t.color_ && style_ == t.style_ && str_ == t.str_;
	}
	bool operator!=(text_surface const &t) const { return !operator==(t); }

private:
	void hash();

private:
	int hash_;
	int font_size_;
	SDL_Color color_;
	int style_;
	mutable int w_, h_;
	std::string str_;
	mutable bool initialized_;
	mutable std::vector<text_chunk> chunks_;
	mutable std::vector<surface> surfs_;
};

text_surface::text_surface(std::string const &str, int size,
		SDL_Color color, int style) :
	hash_(0),
	font_size_(size),
	color_(color),
	style_(style),
	w_(-1),
	h_(-1),
	str_(str),
	initialized_(false),
	chunks_(),
	surfs_()
{
	hash();
}

void text_surface::hash()
{
	int h = 0;
	for (std::string::const_iterator it = str_.begin(), it_end = str_.end(); it != it_end; ++it) {
		h = ((h << 9) | (h >> (sizeof(int) * 8 - 9))) ^ (*it);
	}
	hash_ = h;
}

void text_surface::measure() const
{
	w_ = 0;
	h_ = 0;

	BOOST_FOREACH (text_chunk const &chunk, chunks_)
	{
		TTF_Font* ttfont = get_font(font_id(chunk.subset, font_size_));
		if (ttfont == NULL) {
			continue;
		}
		font_style_setter const style_setter(ttfont, style_);

		int w, h;
		TTF_SizeUTF8(ttfont, chunk.text.c_str(), &w, &h);
		w_ += w;
		h_ = std::max<int>(h_, h);
	}
}

size_t text_surface::width() const
{
	if (w_ == -1) {
		if (chunks_.empty()) {
			chunks_ = split_text(str_);
		}
		measure();
	}
	return w_;
}

size_t text_surface::height() const
{
	if (h_ == -1) {
		if (chunks_.empty()) {
			chunks_ = split_text(str_);
		}
		measure();
	}
	return h_;
}

std::vector<surface> const &text_surface::get_surfaces() const
{
	if (initialized_) {
		return surfs_;
	}

	initialized_ = true;

	// Impose a maximal number of characters for a text line. Do now draw
	// any text longer that that, to prevent a SDL buffer overflow
	if (width() > max_text_line_width) {
		return surfs_;
	}

	BOOST_FOREACH (text_chunk const &chunk, chunks_)
	{
		TTF_Font* ttfont = get_font(font_id(chunk.subset, font_size_));
		if (ttfont == NULL)
			continue;
		font_style_setter const style_setter(ttfont, style_);

		surface s = surface(TTF_RenderUTF8_Blended(ttfont, chunk.text.c_str(), color_));
		if (!s.null()) {
			surfs_.push_back(s);
		}
	}

	return surfs_;
}

namespace font {

class text_cache
{
public:
	static text_surface &find(text_surface const &t);
	static void resize(unsigned int size);
private:
	typedef std::list< text_surface > text_list;
	static text_list cache_;
	static unsigned int max_size_;
};

text_cache::text_list text_cache::cache_;
unsigned int text_cache::max_size_ = 50;

void text_cache::resize(unsigned int size)
{
	while(size < cache_.size()) {
		cache_.pop_back();
	}
	max_size_ = size;
}


text_surface &text_cache::find(text_surface const &t)
{
	text_list::iterator it_bgn = cache_.begin(), it_end = cache_.end();
	text_list::iterator it = std::find(it_bgn, it_end, t);
	if (it != it_end) {
		cache_.splice(it_bgn, cache_, it);
	} else {
		if (cache_.size() >= max_size_) {
			cache_.pop_back();
		}
		cache_.push_front(t);
	}

	return cache_.front();
}

surface get_rendered_text(const std::string& text, int maximum_width, int font_size, const SDL_Color& color, bool editable)
{
	VALIDATE(maximum_width >= 0 && font_size > 0, null_str);
	if (text.empty()) {
		return surface();
	}
	try {
		if (!maximum_width) {
			maximum_width = INT32_MAX;
		}

		tintegrate integrate(text, maximum_width, font_size, color, editable, false);
		return integrate.get_surface();
	}
	catch (utils::invalid_utf8_exception&) {
		// Invalid UTF-8 string
		return surface();
	}
}

tpoint get_rendered_text_size(const std::string& text, int maximum_width, int font_size, const SDL_Color& color, bool editable)
{
	VALIDATE(maximum_width >= 0 && font_size > 0, null_str);
	if (text.empty() || !maximum_width) {
		return tpoint(0, 0);
	}
	try {
		tintegrate integrate(text, maximum_width, font_size, color, editable, false);
		tpoint size = integrate.get_size();

		VALIDATE(size.x <= maximum_width, null_str);

		return size;
	}
	catch (utils::invalid_utf8_exception&) {
		// Invalid UTF-8 string
		return tpoint(0, 0);
	}
}

static surface text_render(const std::string& text, int font_size, const SDL_Color& font_color, int style)
{
	text_surface txt_surf(text, font_size, font_color, style);
	
	text_surface* const cached_surf = &text_cache::find(txt_surf);
	const std::vector<surface>& surfs = cached_surf->get_surfaces();

	surface ret;
	if (surfs.empty()) {
		return ret;
	}

	// we keep blank lines and spaces (may be wanted for indentation)
	if (surfs.size() == 1) {
		ret = surfs.front();

	} else {
		size_t width = cached_surf->width();
		size_t height = cached_surf->height();

		ret = create_neutral_surface(width, height);
		VALIDATE(ret, null_str);

		size_t xpos = 0;
		for (std::vector<surface>::const_iterator it = surfs.begin(), it_end = surfs.end(); it != it_end; ++ it) {
			const surface& surf = *it;
			SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE); // direct blit without alpha blending
			SDL_SetSurfaceRLE(surf, 0);
			SDL_Rect dstrect = create_rect(xpos, 0, 0, 0);
			sdl_blit(surf, NULL, ret, &dstrect);
			xpos += surf->w;
		}
	}

	SDL_SetSurfaceBlendMode(ret, SDL_BLENDMODE_BLEND);
	SDL_SetSurfaceRLE(ret, 0);
	return ret;
}

// it is called by tintegrate only!
surface get_single_line_surface(const std::string& text, int font_size, const SDL_Color& color, int style)
{
	if (!font_size) {
		return surface();
	}
	if (text.empty()) {
		return surface();
	}
	VALIDATE(!strchr(text.c_str(), '\n'), null_str);

	try {
		return text_render(text, font_size, color, style);
	}
	catch (utils::invalid_utf8_exception&) {
		// Invalid UTF-8 string
		return surface();
	}
}

int get_max_height(int size)
{
	// Only returns the maximal size of the first font
	TTF_Font* const font = get_font(font_id(0, size));
	if (font == NULL) {
		return 0;
	}
	return TTF_FontHeight(font);
}

int line_width(const std::string& line, int font_size, int style)
{
	return line_size(line, font_size, style).w;
}

int line_minus1_width(const std::string& text, int font_size, int style)
{
	if (text.empty()) {
		return 0;
	}
	size_t last_char_len = 0;
	for (utils::utf8_iterator it(text); it != utils::utf8_iterator::end(text); ++ it) {
		last_char_len = it.substr().second - it.substr().first;
	}
	return line_width(text.substr(0, text.size() - last_char_len), font_size, style);
}

SDL_Rect line_size(const std::string& line, int font_size, int style)
{
	line_size_cache_map& cache = line_size_cache[style][font_size];

	const line_size_cache_map::const_iterator i = cache.find(line);
	if (i != cache.end()) {
		return i->second;
	}

	SDL_Rect res;

	const SDL_Color col = { 0, 0, 0, 0 };
	text_surface s(line, font_size, col, style);

	res.w = s.width();
	res.h = s.height();
	res.x = res.y = 0;

	cache.insert(std::pair<std::string,SDL_Rect>(line,res));
	return res;
}

std::string make_text_ellipsis(const std::string &text, int font_size, int max_width, int style)
{
	static const std::string ellipsis = "...";
	const int ellipsis_width = line_width(ellipsis, font_size, style);

	if (ellipsis_width > max_width) {
		return "";
	}

	tintegrate integrate(text, INT_MAX, font_size, font::BIGMAP_COLOR, true, false);
	if (integrate.get_size().x <= max_width) {
		return text;
	}

	// !!bug, handle_selection will discard markup.
	std::string cut = integrate.handle_selection(0, 0, max_width - ellipsis_width, 0, nullptr);
	return cut + ellipsis;
}

std::string make_text_ellipsis(const std::string& text, size_t max_count)
{
	static const std::string ellipsis = "...";

	if (text.size() <= max_count) {
		return text;
	}
	if (ellipsis.size() > max_count) {
		return "";
	}

	std::string current_substring;

	utils::utf8_iterator itor(text);

	size_t count = 0;
	for (; itor != utils::utf8_iterator::end(text); ++ itor) {
		std::string tmp = current_substring;
		tmp.append(itor.substr().first, itor.substr().second);

		current_substring.append(itor.substr().first, itor.substr().second);

		if ( ++ count >= max_count) {
			return current_substring + ellipsis;
		}
	}

	return text; // Should not happen
}

}

namespace {

typedef std::map<int, font::floating_label> label_map;
label_map labels;
int label_id = 1;

std::stack<std::set<int> > label_contexts;
}


namespace font {

floating_label::floating_label(const std::string& text)
		: surf_(NULL), buf_(NULL), text_(text),
		font_size_(SIZE_DEFAULT),
		color_(NORMAL_COLOR),	bgcolor_(), bgalpha_(0),
		xpos_(0), ypos_(0),
		xmove_(0), ymove_(0), lifetime_(-1),
		width_(-1),
		clip_rect_(screen_area()),
		alpha_change_(0), visible_(true), align_(CENTER_ALIGN),
		border_(0), scroll_(ANCHOR_LABEL_SCREEN), use_markup_(true)
{}

void floating_label::set_lifetime(int lifetime) 
{
	lifetime_ = lifetime;
	alpha_change_ = -255 / lifetime_;
}

void floating_label::move(double xmove, double ymove)
{
	xpos_ += xmove;
	ypos_ += ymove;
}

int floating_label::xpos(size_t width) const
{
	int xpos = int(xpos_);
	if(align_ == font::CENTER_ALIGN) {
		xpos -= width/2;
	} else if(align_ == font::RIGHT_ALIGN) {
		xpos -= width;
	}

	return xpos;
}

surface floating_label::create_surface()
{
	if (surf_.null()) {
		tintegrate integrate(text_, width_ < 0 ? clip_rect_.w : width_, font_size_, color_, false, false);
		surface foreground = integrate.get_surface();

		if (foreground == NULL) {
			// could not create floating label's text
			return NULL;
		}

		// combine foreground text with its background
		if (bgalpha_ != 0) {
			// background is a dark tootlip box
			surface background = create_neutral_surface(foreground->w + border_*2, foreground->h + border_*2);

			VALIDATE(background, null_str);

			Uint32 color = SDL_MapRGBA(foreground->format, bgcolor_.r,bgcolor_.g, bgcolor_.b, bgalpha_);
			sdl_fill_rect(background,NULL, color);

			// we make the text less transparent, because the blitting on the
			// dark background will darken the anti-aliased part.
			// This 1.13 value seems to restore the brightness of version 1.4
			// (where the text was blitted directly on screen)
			foreground = adjust_surface_alpha(foreground, ftofxp(1.13), false);

			SDL_Rect r = create_rect( border_, border_, 0, 0);
			SDL_SetSurfaceBlendMode(foreground, SDL_BLENDMODE_BLEND);
			SDL_SetSurfaceRLE(foreground, 0);

			sdl_blit(foreground, NULL, background, &r);

			surf_ = makesure_neutral_surface(background);
			// RLE compression seems less efficient for big semi-transparent area
			// so, remove it for this case, but keep the optimized display format
			SDL_SetSurfaceBlendMode(surf_, SDL_BLENDMODE_BLEND);
			SDL_SetSurfaceRLE(surf_, 0);

		} else {
			// background is blurred shadow of the text
			surface background = create_neutral_surface
				(foreground->w + 4, foreground->h + 4);
			sdl_fill_rect(background, NULL, 0);
			SDL_Rect r = { 2, 2, 0, 0 };
			sdl_blit(foreground, NULL, background, &r);
			background = shadow_image(background, false);

			if (background == NULL) {
				// could not create floating label's shadow
				surf_ = makesure_neutral_surface(foreground);
				return surf_;
			}
			SDL_SetSurfaceBlendMode(foreground, SDL_BLENDMODE_BLEND);
			sdl_blit(foreground, NULL, background, &r);
			surf_ = makesure_neutral_surface(background);
		}
	}

	return surf_;
}

void floating_label::draw(texture& screen)
{
	if (!visible_) {
		buf_ = NULL;
		return;
	}

	create_surface();
	if (surf_ == NULL) {
		return;
	}

	SDL_Rect rect = create_rect(xpos(surf_->w), ypos_, surf_->w, surf_->h);
	SDL_Renderer* renderer = get_renderer();
	const texture_clip_rect_setter clip_setter(&clip_rect_);

	texture_from_texture(screen, buf_, &rect, 0, 0);

	render_surface(renderer, surf_, NULL, &rect);
}

void floating_label::undraw(texture& screen)
{
	if (screen == NULL || buf_.get() == NULL) {
		return;
	}

	SDL_Rect rect = create_rect(xpos(surf_->w), ypos_, surf_->w, surf_->h);
	const texture_clip_rect_setter clip_setter(&clip_rect_);

	SDL_RenderCopy(get_renderer(), buf_.get(), NULL, &rect);

	move(xmove_,ymove_);
	if (lifetime_ > 0) {
		-- lifetime_;
		if (alpha_change_ != 0 && (xmove_ != 0.0 || ymove_ != 0.0) && surf_ != NULL) {
			// fade out moving floating labels
			// note that we don't optimize these surfaces since they will always change
			surf_.assign(adjust_surface_alpha_add(surf_,alpha_change_,false));
		}
	}
}

int add_floating_label(const floating_label& flabel)
{
	if(label_contexts.empty()) {
		return 0;
	}

	++label_id;
	labels.insert(std::pair<int, floating_label>(label_id, flabel));
	label_contexts.top().insert(label_id);
	return label_id;
}

void move_floating_label(int handle, double xmove, double ymove)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		i->second.move(xmove,ymove);
	}
}

void scroll_floating_labels(double xmove, double ymove)
{
	for(label_map::iterator i = labels.begin(); i != labels.end(); ++i) {
		if(i->second.scroll() == ANCHOR_LABEL_MAP) {
			i->second.move(xmove,ymove);
		}
	}
}

void remove_floating_label(int handle)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		if(label_contexts.empty() == false) {
			label_contexts.top().erase(i->first);
		}

		labels.erase(i);
	}
}

void show_floating_label(int handle, bool value)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		i->second.show(value);
	}
}

SDL_Rect get_floating_label_rect(int handle)
{
	const label_map::iterator i = labels.find(handle);
	if(i != labels.end()) {
		const surface surf = i->second.create_surface();
		if(surf != NULL) {
			return create_rect(0, 0, surf->w, surf->h);
		}
	}

	return empty_rect;
}

floating_label_context::floating_label_context()
{
	draw_floating_labels();

	label_contexts.push(std::set<int>());
}

floating_label_context::~floating_label_context()
{
	const std::set<int>& labels = label_contexts.top();
	for(std::set<int>::const_iterator i = labels.begin(); i != labels.end(); ) {
		remove_floating_label(*i++);
	}

	label_contexts.pop();

	undraw_floating_labels();
}

void draw_floating_labels()
{
	if (label_contexts.empty()) {
		return;
	}

	const std::set<int>& context = label_contexts.top();

	//draw the labels in the order they were added, so later added labels (likely to be tooltips)
	//are displayed over earlier added labels.
	texture screen = get_screen_texture();
	for (label_map::iterator i = labels.begin(); i != labels.end(); ++i) {
		if (context.count(i->first) > 0) {
			i->second.draw(screen);
		}
	}
}

void undraw_floating_labels()
{
	if(label_contexts.empty()) {
		return;
	}

	std::set<int>& context = label_contexts.top();

	//undraw labels in reverse order, so that a LIFO process occurs, and the screen is restored
	//into the exact state it started in.
	texture screen = get_screen_texture();
	for(label_map::reverse_iterator i = labels.rbegin(); i != labels.rend(); ++i) {
		if(context.count(i->first) > 0) {
			i->second.undraw(screen);
		}
	}

	//remove expired labels
	for(label_map::iterator j = labels.begin(); j != labels.end(); ) {
		if(context.count(j->first) > 0 && j->second.expired()) {
			context.erase(j->first);
			labels.erase(j++);
		} else {
			++j;
		}
	}
}

}

static bool add_font_to_fontlist(config &fonts_config,
	std::vector<font::subset_descriptor>& fontlist, const std::string& name)
{
	config &font = fonts_config.find_child("font", "name", name);
	if (!font)
		return false;

		fontlist.push_back(font::subset_descriptor());
		fontlist.back().name = name;
		std::vector<std::string> ranges = utils::split(font["codepoints"]);

		for(std::vector<std::string>::const_iterator itor = ranges.begin();
				itor != ranges.end(); ++itor) {

			std::vector<std::string> r = utils::split(*itor, '-');
			if(r.size() == 1) {
				size_t r1 = lexical_cast_default<size_t>(r[0], 0);
				fontlist.back().present_codepoints.push_back(std::pair<size_t, size_t>(r1, r1));
			} else if(r.size() == 2) {
				size_t r1 = lexical_cast_default<size_t>(r[0], 0);
				size_t r2 = lexical_cast_default<size_t>(r[1], 0);

				fontlist.back().present_codepoints.push_back(std::pair<size_t, size_t>(r1, r2));
			}
		}

		return true;
	}

namespace font {


bool load_font_config()
{
	//read font config separately, so we do not have to re-read the whole
	//config when changing languages
	config cfg;
	try {
		scoped_istream stream = preprocess_file(get_wml_location("hardwired/fonts.cfg"));
		read(cfg, *stream);
	} catch (config::error &e) {
		// could not read fonts.cfg: $e.message
		return false;
	}

	config &fonts_config = cfg.child("fonts");
	if (!fonts_config)
		return false;

	std::set<std::string> known_fonts;
	BOOST_FOREACH (const config &font, fonts_config.child_range("font")) {
		known_fonts.insert(font["name"]);
	}

	const std::vector<std::string> font_order = utils::split(fonts_config["order"]);
	std::vector<font::subset_descriptor> fontlist;
	std::vector<std::string>::const_iterator font;
	for(font = font_order.begin(); font != font_order.end(); ++font) {
		add_font_to_fontlist(fonts_config, fontlist, *font);
		known_fonts.erase(*font);
	}
	std::set<std::string>::const_iterator kfont;
	for(kfont = known_fonts.begin(); kfont != known_fonts.end(); ++kfont) {
		add_font_to_fontlist(fonts_config, fontlist, *kfont);
	}

	if(fontlist.empty())
		return false;

	font::set_font_list(fontlist);
	return true;
}

}

int font_hdpi_size_from_cfg_size(int cfg_size)
{
	int diff = cfg_size - font_cfg_reference_size;
	VALIDATE(posix_abs(diff) <= font_max_cfg_size_diff, null_str);

	if (diff == 0) {
		return font::SIZE_DEFAULT;
	}
	if (diff == -1) {
		return font::SIZE_SMALL;
	}
	if (diff == -2) {
		return font::SIZE_SMALLER;
	}
	if (diff == 1) {
		return font::SIZE_LARGE;
	}
	// diff == 2
	return font::SIZE_LARGER;
}