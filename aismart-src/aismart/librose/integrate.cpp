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
#include "marked-up_text.hpp"
#include "serialization/parser.hpp"
#include "integrate.hpp"
#include "wml_exception.hpp"
#include "image.hpp"
#include "font.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include "theme.hpp"
#include "area_anim.hpp"
#include "gui/widgets/widget.hpp"

#include <list>
#include <set>

namespace {
	const int box_width = 2;
}

namespace ht {

static std::string align_to_string(tintegrate::ALIGNMENT align)
{
	if (align == tintegrate::LEFT) return "left";
	if (align == tintegrate::MIDDLE) return "middle";
	if (align == tintegrate::RIGHT) return "right";
	if (align == tintegrate::BACK) return "back";
	if (align == tintegrate::HERE) return "";
	return "";
}

std::string generate_img(const std::string& src, tintegrate::ALIGNMENT align, bool floating, const std::map<std::string, std::string>& extra)
{
	std::stringstream ss;

	ss << "<img>src=\"" << src << "\"";
	std::string str = align_to_string(align);
	if (!str.empty()) {
		ss << " align=\"";
		ss << str << "\"";
		
	}
	if (floating) {
		ss << " float=yes";
	}
	for (std::map<std::string, std::string>::const_iterator it = extra.begin(); it != extra.end(); ++ it) {
		ss << " " << it->first << "=\"" << it->second << "\"";
	}
/*
	strstr << " box=no";
*/
	ss << "</img>";
/*
	if (floating) {
		strstr << "<jump>to=" << "0";
		strstr << "</jump>";
	}
*/
	return ss.str();
}

std::string generate_format(const std::string& text, uint32_t color, int font_size, bool bold, bool italic, const std::map<std::string, std::string>& extra)
{
	std::stringstream ss;

	if (text.empty()) {
		return null_str;
	}
	// text maybe have sapce character.
	ss << "<format>text=\"" << tintegrate::stuff_escape(text, true) << "\"";
	if (color) {
		ss << " color=\"";
		if (color & 0xff000000) {
			ss << encode_color(color);
		} else {
			ss << color ;
		}
		ss << "\"";
	}
	if (font_size) {
		ss << " font_size=" << font_size;
	}
	if (bold) {
		ss << " bold=yes";
	}
	if (italic) {
		ss << " italic=yes";
	}
	for (std::map<std::string, std::string>::const_iterator it = extra.begin(); it != extra.end(); ++ it) {
		ss << " " << it->first << "=\"" << it->second << "\"";
	}

	ss << "</format>";

	return ss.str();
}

std::string generate_format(int val, uint32_t color, int font_size, bool bold, bool italic, const std::map<std::string, std::string>& extra)
{
	std::stringstream ss;

	// text maybe have sapce character.
	ss << "<format>text=\"" << val << "\"";
	if (color) {
		ss << " color=\"";
		if (color & 0xff000000) {
			ss << encode_color(color);
		} else {
			ss << color ;
		}
		ss << "\"";
	}
	if (font_size) {
		ss << " font_size=" << font_size;
	}
	if (bold) {
		ss << " bold=yes";
	}
	if (italic) {
		ss << " italic=yes";
	}
	for (std::map<std::string, std::string>::const_iterator it = extra.begin(); it != extra.end(); ++ it) {
		ss << " " << it->first << "=\"" << it->second << "\"";
	}
	ss << "</format>";

	return ss.str();
}

}

tintegrate::titem::titem(const tintegrate& integrate, int index, surface surface, int markup_pos, int pos, bool quote_require_escape, int x, int y, 
			const std::string& _text, const std::string& reference_to, int font_size, int style, int src_size,
			bool _floating, bool _box, ALIGNMENT alignment)
	: integrate_(integrate)
	, index(index)
	, rect()
	, surf(surface)
	, anim(null_cfg)
	, markup_pos(markup_pos)
	, pos(pos)
	, quote_require_escape(quote_require_escape)
	, text(_text)
	, ref_to(reference_to)
	, font_size(font_size)
	, style(style)
	, src_size(src_size)
	, floating(_floating)
	, box(_box)
	, align(alignment)
{
	rect.x = x;
	rect.y = y;
	if (surface) {
		rect.w = box ? surface->w + box_width * 2 : surface->w;
		rect.h = box ? surface->h + box_width * 2 : surface->h;
	} else {
		rect.w = 0;
		rect.h = font_size;
	}
}

tintegrate::titem::titem(const tintegrate& integrate, int index, surface surface, int pos, int x, int y, int src_size, bool _floating, bool _box, ALIGNMENT alignment)
	: integrate_(integrate)
	, index(index)
	, rect()
	, surf(surface)
	, anim(null_cfg)
	, markup_pos(pos)
	, pos(pos)
	, text("")
	, ref_to("")
	, font_size(font::SIZE_DEFAULT)
	, style(TTF_STYLE_NORMAL)
	, src_size(src_size)
	, floating(_floating)
	, box(_box)
	, align(alignment)
	, quote_require_escape(false)
{
	rect.x = x;
	rect.y = y;
	rect.w = box ? surface->w + box_width * 2 : surface->w;
	rect.h = box ? surface->h + box_width * 2 : surface->h;
}

tintegrate::titem::titem(const tintegrate& integrate, const config& anim, int pos, const SDL_Rect& rect, int src_size, bool _floating, bool _box, ALIGNMENT alignment) 
	: integrate_(integrate)
	, index(0)
	, rect(rect)
	, anim(anim)
	, markup_pos(pos)
	, pos(pos)
	, text("")
	, ref_to("")
	, font_size(font::SIZE_DEFAULT)
	, style(TTF_STYLE_NORMAL)
	, src_size(src_size)
	, floating(_floating)
	, box(_box)
	, align(alignment)
	, quote_require_escape(false)
{
}

bool tintegrate::titem::text_type() const 
{ 
	if (quote_require_escape && integrate_.atom_markup_) {
		return false;
	}
	return !text.empty() || !rect.w;
}

uint32_t tintegrate::hero_color = 0xff00ff00;
uint32_t tintegrate::object_color = 0xffffff00; // yellow
uint32_t tintegrate::tactic_color = 0xff0000ff;
double tintegrate::screen_ratio = 1;
const char* tintegrate::require_escape = "\\\"";
const std::string tintegrate::anim_tag_prefix = "a_n_i_m/";
std::set<std::string> tintegrate::support_markups;

bool tintegrate::is_require_escape_unicode(wchar_t ch)
{
	if (ch > 0xff) {
		return false;
	}
	return !!strchr(require_escape, ch);
}

std::string tintegrate::stuff_escape(const std::string& str, bool quote_require_escape)
{
	if (!quote_require_escape) {
		return str;
	}

	std::string ret;
	for (utils::utf8_iterator it(str); it != utils::utf8_iterator::end(str); ++ it) {
		wchar_t ch = *it;
		if (is_require_escape_unicode(ch)) {
			ret.append("\\");
		}
		ret.append(it.substr().first, it.substr().second);
	}
	return ret;
}

std::string tintegrate::drop_escape(const std::string& str, bool quote_require_escape)
{
	if (!quote_require_escape) {
		return str;
	}

	const char escape_char = '\\';
	std::stringstream ss;
	const char* c_str = str.c_str();
	size_t size = str.size();
	char ch;
	for (size_t i = 0; i < size; i ++) {
		ch = c_str[i];
		if (ch == escape_char) {
			ch = c_str[++ i];
		}
		ss << ch;
	}
	return ss.str();
}

tintegrate* share_canvas_integrate = NULL;

tintegrate::tintegrate(const std::string& src, int maximum_width, int default_font_size, const SDL_Color& default_font_color, bool editable, bool atom_markup)
	: src_(editable? src: null_str)
	, editable_(editable)
	, atom_markup_(atom_markup)
	, items_()
	, last_row_()
	, title_spacing_(16)
	, curr_loc_(0, 0)
	, min_row_height_(font::get_max_height(default_font_size))
	, curr_row_height_(min_row_height_)
	, original_maximum_width_(maximum_width)
	, maximum_width_(maximum_width)
	, default_font_size_(default_font_size)
	, default_font_color_(default_font_color)
	, align_bottom_(true)
	, exist_anim_(false)
	, anims_()
	, bubble_anims_()
{
	if (support_markups.empty()) {
		support_markups.insert("ref");
		support_markups.insert("img");
		support_markups.insert("jump");
		support_markups.insert("format");
	}

	VALIDATE(maximum_width_ > 0 && default_font_size_ > 0, null_str);
	// in order to align with hdpi_scale, both width and height returned by tintegrate must always ceil align width hdpi_scale!
	// because ceil, it maybe result returned width is larger than maximum_width_. for exmaple below:
	// maximum_width_ = 1011,  hdpi_scale = 2. 
	// ==> calculated width is 1011, next ceil align with hdpi_scale, returned width is 1012!
	// so first, make maximum_width_ floor align with hdpi_scale. 
	maximum_width_ = posix_align_floor(maximum_width_, gui2::twidget::hdpi_scale);

	if (editable_) {
		// must not include '\r'. '\r' will make backspace/delete complex!
		src_.erase(std::remove(src_.begin(), src_.end(), '\r'), src_.end());
	}
	const std::string& src2 = editable_? src_: src;

	std::map<int, utils::tcfg_string_pair> parsed_items;
	utils::split_integrate_src(src2, support_markups, parsed_items);

	std::map<int, utils::tcfg_string_pair>::const_iterator it;
	for (it = parsed_items.begin(); it != parsed_items.end(); ++it) {
		const int segment_start = it->first;
		const utils::tcfg_string_pair& segment = it->second;

		if (segment.iscfg) {

#define TRY(name) do { \
			if (segment.text == #name) \
				handle_##name##_cfg(it->first, segment.cfg); \
			} while (0)

			TRY(ref);
			TRY(img);
			TRY(jump);
			TRY(format);
#undef TRY

		} else {
			add_text_item(0, segment_start, segment_start, false, true, segment.text, default_font_color_);
		}
	}

	if (!src2.empty() && items_.back().src_end_is_lf(src2)) {
		// pos = src.size()
		// src_size = 0
		// text = ""
		add_item(titem(*this, 0, surface(), src2.size(), src2.size(), false, curr_loc_.first, curr_loc_.second, null_str, null_str, curr_row_height_, TTF_STYLE_NORMAL, 0));
	}
	down_one_line(); // End the last line.
}

tintegrate::~tintegrate()
{
	if (!bubble_anims_.empty() || !anims_.empty()) {
		for (std::map<int, int>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
			anim2::manager::instance->erase_area_anim(it->second);
		}
	}
}

void tintegrate::set_default_font_color(uint32_t color)
{
	default_font_color_ = uint32_to_color(color);
}

int tintegrate::forward_pos(int start, const std::string& key) const
{
	if (!editable_ || src_[start] != '<') {
		return start;
	}
	int pos = src_.find(key, start) + key.size();
	while (src_[pos] != '=') { pos ++; }
	// if space, see it into key value.
	if (src_[++ pos] == '"') {
		pos ++;
	}

	return pos;
}

void tintegrate::handle_ref_cfg(int tag_start, const config &cfg)
{
	const std::string dst = cfg["dst"];
	const std::string text = cfg["text"];
	bool force = cfg["force"].to_bool();

	if (dst == "") {
		std::stringstream msg;
		msg << "Ref markup must have dst attribute. Please submit a bug"
		       " report if you have not modified the game files yourself. Erroneous config: ";
		write(msg, cfg);
		VALIDATE(false, msg.str());
	}

	int start = forward_pos(tag_start, "text");
	if (force || (!help::book_toplevel || help::find_topic(*help::book_toplevel, dst))) {
		add_text_item(0, tag_start, start, true, true, text, default_font_color_, dst);

	} else {
		// detect the broken link but quietly silence the hyperlink for normal user
		add_text_item(0, tag_start, start, true, true, text, default_font_color_, "", true);
	}

}

void tintegrate::handle_img_cfg(int start, const config &cfg)
{
	const std::string src = cfg["src"];
	const std::string align = cfg["align"];
	bool floating = cfg["float"].to_bool();
	bool box = cfg["box"].to_bool(true);
	VALIDATE(!src.empty(), "Img markup must have src attribute.");

	// start = forward_pos(start, "src");
	add_img_item(0, start, src, align, floating, box, cfg);
}

void tintegrate::handle_jump_cfg(int, const config &cfg)
{
	const std::string amount_str = cfg["amount"];
	const std::string to_str = cfg["to"];
	VALIDATE(!amount_str.empty() || !to_str.empty(), "Jump markup must have either a to or an amount attribute.");
	unsigned jump_to = curr_loc_.first;
	if (amount_str != "") {
		unsigned amount;
		try {
			amount = lexical_cast<unsigned, std::string>(amount_str);
		}
		catch (bad_lexical_cast) {
			VALIDATE(false, "Invalid amount the amount attribute in jump markup.");
		}
		jump_to += amount;
	}
	if (to_str != "") {
		unsigned to;
		try {
			to = lexical_cast<unsigned, std::string>(to_str);
		}
		catch (bad_lexical_cast) {
			VALIDATE(false, "Invalid amount in the to attribute in jump markup.");
		}
		if (to < jump_to) {
			down_one_line();
		}
		jump_to = to;
	}
	if (jump_to != 0 && static_cast<int>(jump_to) <
            get_max_x(curr_loc_.first, curr_row_height_)) {

		curr_loc_.first = jump_to;
	}
}

void tintegrate::handle_format_cfg(int tag_start, const config &cfg)
{
	const std::string text = cfg["text"];
	if (text == "") {
		// sorry, cannot avoid empty text
		// throw parse_error("Format markup must have text attribute.");
		return;
	}
	if (atom_markup_) {
		const size_t pos = text.find_first_of("\r\n");
		VALIDATE(pos == std::string::npos, null_str);
	}

	bool bold = cfg["bold"].to_bool();
	bool italic = cfg["italic"].to_bool();
	int font_size = cfg["font_size"].to_int(default_font_size_);

	SDL_Color color = default_font_color_;

	if (cfg.has_attribute("color")) {
		std::string str = cfg["color"].str();
		if (str.find(',') != std::string::npos) {
			color = uint32_to_color(decode_color(str));
		} else {
			uint32_t tmp = utils::to_size_t(str);
			int tpl, index;
			ht_split_theme_color(tmp, tpl, index);
			if (tpl >= 0 && tpl < theme::color_tpls && index >= 0 && index < theme::tpl_colors) {
				color = uint32_to_color(theme::text_color_from_index(tpl, index));
			}
		}
	}
	int start = forward_pos(tag_start, "text");
	add_text_item(0, tag_start, start, true, true, text, color, "", false, font_size, bold, italic);
}

void tintegrate::add_text_item(int index, int markup_start, int start, bool quote_require_escape, bool first, const std::string& text, const SDL_Color& text_color,
		const std::string& ref_dst, bool broken_link, int _font_size, bool bold, bool italic)
{
	const int font_size = _font_size <= 0 ? default_font_size_ : _font_size;
	if (text.empty()) {
		return;
	}

	if (text[0] == '\n') {
		const int src_text_size = get_src_text_size(start, null_str, quote_require_escape);
		VALIDATE(!editable_ || src_text_size == 1, null_str); // 1 is '\n'.

		if (first || last_row_.empty()) {
			// this line has \n only.
			if (editable_) {
				validate_str(start, src_text_size, "\n", quote_require_escape);
			}
			add_item(titem(*this, index ++, surface(), markup_start, start, quote_require_escape, curr_loc_.first, curr_loc_.second, null_str, null_str, curr_row_height_, TTF_STYLE_NORMAL, src_text_size));
		}
		start += src_text_size;

		down_one_line();
		const std::string rest_text = text.substr(1);
		add_text_item(index, start, start, quote_require_escape, false, rest_text, text_color, ref_dst, broken_link, _font_size, bold, italic);
		return;
	}

	int style = ref_dst == "" ? 0 : TTF_STYLE_UNDERLINE;
	style |= bold ? TTF_STYLE_BOLD : 0;
	style |= italic ? TTF_STYLE_ITALIC : 0;
	const int remaining_width = get_remaining_width();
	const std::string first_word = remaining_width > 0? help::get_first_word(text, font_size, style, remaining_width): null_str;

	if (!last_row_.empty() && (first_word.empty() || remaining_width < font::line_width(first_word, font_size, style))) {
		// The first word does not fit, and we are not at the start of
		// the line. Move down.
		down_one_line();
		std::string s = editable_? text: help::remove_first_space(text);
		add_text_item(index, markup_start, start, quote_require_escape, false, s, text_color, ref_dst, broken_link, _font_size, bold, italic);
	} else {
		std::vector<std::string> parts = help::split_in_width(text, font_size, style, remaining_width);

		const std::string& first_part = parts.front();
		// Always override the color if we have a cross reference.
		SDL_Color color;
		if (ref_dst.empty()) {
			color = text_color;
		} else if (broken_link) {
			color = font::BAD_COLOR;
		} else {
			color = font::YELLOW_COLOR;
		}

		surface surf(font::get_single_line_surface(first_part, font_size, color, style));
		const int src_text_size = get_src_text_size(start, first_part, quote_require_escape);
		if (editable_) {
			std::string text = first_part;
			if (get_src_text_size2(start, text, quote_require_escape) < src_text_size) {
				text.append("\n");
			}
			validate_str(start, src_text_size, text, quote_require_escape);
		}

		// for example first_part = "\r", surf will null.
		if (surf.get()) {
			// [See remark#22]
			SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_NONE); // direct blit without alpha blending
			SDL_SetSurfaceRLE(surf, 0);
		}
		add_item(titem(*this, index ++, surf, markup_start, start, quote_require_escape, curr_loc_.first, curr_loc_.second, first_part, ref_dst, font_size, style, src_text_size));

		start += src_text_size;

		if (editable_) {
			// \r will be processed by next add_text_item.
			start -= items_.back().src_end_is_lf(src_);
		}

		if (parts.size() > 1) {
			const std::string& s = parts.back();
			add_text_item(index, start, start, quote_require_escape, false, s, text_color, ref_dst, broken_link, _font_size, bold, italic);
		}
	}
}

surface adaptive_scale_image(surface& surf, double max_ratio)
{
	if (max_ratio <= 1) {
		return surf;
	}

	// 1 or 2
	if (max_ratio > 2) {
		max_ratio = 2;
	}
	int width = surf->w;
	int height = surf->h;
	int min = max_ratio <= 1.5? 64: 48;

	if (width <= min || height <= min) {
		return surf;
	}

	double min_ratio = std::min<double>(1.0 * width / min, 1.0 * height / min);
	min_ratio = std::min<double>(min_ratio, max_ratio);

	return scale_surface(surf, surf->w / min_ratio, surf->h / min_ratio);
}

void tintegrate::add_img_item(int index, int start, const std::string& path, const std::string& alignment,
								  const bool floating, const bool box, const config& cfg)
{
	surface surf;
	std::string anim_id;
	int width, height;

	size_t pos = path.find(anim_tag_prefix);
	if (pos == std::string::npos) {
		surf = image::get_image(image::locator(get_hdpi_name(path, gui2::twidget::hdpi_scale)));
		if (!surf) {
			surf = image::get_image(path);
		}
		if (surf.null()) {
			return;
		}

		if (screen_ratio > 1) {
			surf = adaptive_scale_image(surf, screen_ratio);
		}
		width = surf->w + (box ? box_width * 2 : 0);
		height = surf->h + (box ? box_width * 2 : 0);

	} else {
		anim_id = path.substr(pos + anim_tag_prefix.size());
		int type = anim2::find(anim_id);
		if (type == nposm) {
			return;
		}
		width = cfg["width"].to_int();
		height = cfg["height"].to_int();
		VALIDATE(width > 0 && height > 0, "Anim markup must have width and height attribute.");
	}

	ALIGNMENT align = str_to_align(alignment);
	if (align == BACK && items_.empty()) {
		align = HERE;
	}
	if (align == HERE && floating) {
		align = LEFT;
	}
	int xpos;
	int ypos = curr_loc_.second;
	int text_width = maximum_width_;
	switch (align) {
	case HERE:
		xpos = curr_loc_.first;
		break;
	case LEFT:
	default:
		xpos = 0;
		break;
	case MIDDLE:
		xpos = text_width / 2 - width / 2 - (box ? box_width : 0);
		break;
	case RIGHT:
		xpos = text_width - width - (box ? box_width * 2 : 0);
		break;
	case BACK:
		xpos = items_.back().rect.x;
		ypos = items_.back().rect.y;
		break;
	}
	if (align != BACK && curr_loc_.first != get_min_x(curr_loc_.second, curr_row_height_)
		&& (xpos < curr_loc_.first || xpos + width > text_width)) {
		down_one_line();
		add_img_item(index, start, path, alignment, floating, box, cfg);
	} else {
		if (!floating) {
			curr_loc_.first = xpos;
		} else {
			ypos = get_y_for_floating_img(width, xpos, ypos);
		}
		// calculate size in src
		const std::string key = "</img>";
		int src_size = src_.find(key, start) + key.size() - start;
		if (surf) {
			add_item(titem(*this, index ++, surf, start, xpos, ypos, src_size, floating, box, align));
		} else {
			config anim_cfg = cfg;
			anim_cfg["id"] = anim_id;
			add_item(titem(*this, anim_cfg, start, create_rect(xpos, ypos, width, height), src_size, floating, box, align));
			exist_anim_ = true;
		}
	}
}

int tintegrate::get_y_for_floating_img(const int width, const int x, const int desired_y)
{
	int min_y = desired_y;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const titem& itm = *it;
		if (itm.floating) {
			if ((itm.rect.x + itm.rect.w > x && itm.rect.x < x + width)
				|| (itm.rect.x > x && itm.rect.x < x + width)) {
				min_y = std::max<int>(min_y, itm.rect.y + itm.rect.h);
			}
		}
	}
	return min_y;
}

int tintegrate::get_min_x(const int y, const int height)
{
	int min_x = 0;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const titem& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.rect.h > y && itm.align == LEFT) {
				min_x = std::max<int>(min_x, itm.rect.w + 5);
			}
		}
	}
	return min_x;
}

int tintegrate::get_max_x(const int y, const int height)
{
	int text_width = maximum_width_;
	int max_x = text_width;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++it) {
		const titem& itm = *it;
		if (itm.floating) {
			if (itm.rect.y < y + height && itm.rect.y + itm.rect.h > y) {
				if (itm.align == RIGHT) {
					max_x = std::min<int>(max_x, text_width - itm.rect.w - 5);
				} else if (itm.align == MIDDLE) {
					max_x = std::min<int>(max_x, text_width / 2 - itm.rect.w / 2 - 5);
				}
			}
		}
	}
	return max_x;
}

void tintegrate::add_item(const titem &itm)
{
	items_.push_back(itm);
	if (!itm.floating) {
		curr_loc_.first += itm.rect.w;
		curr_row_height_ = std::max<int>(itm.rect.h, curr_row_height_);
		last_row_.push_back(&items_.back());
	}
	else {
		if (itm.align == LEFT) {
			curr_loc_.first = itm.rect.w + 5;
		}
	}
}


tintegrate::ALIGNMENT tintegrate::str_to_align(const std::string &cmp_str)
{
	if (cmp_str == "left") {
		return LEFT;
	} else if (cmp_str == "middle") {
		return MIDDLE;
	} else if (cmp_str == "right") {
		return RIGHT;
	} else if (cmp_str == "back") {
		return BACK;
	} else if (cmp_str == "here" || cmp_str == "") { // Make the empty string be "here" alignment.
		return HERE;
	}

	std::stringstream msg;
	msg << "Invalid alignment string: '" << cmp_str << "'";
	VALIDATE(false, msg.str());
	return LEFT;
}

void tintegrate::down_one_line()
{
	adjust_last_row();
	last_row_.clear();
	// curr_loc_.second += curr_row_height_ + (curr_row_height_ == min_row_height_ ? 0 : 2);
	curr_loc_.second += curr_row_height_ + (curr_row_height_ == min_row_height_ ? 0 : 0);
	// [see remark#25]
	VALIDATE(curr_loc_.first <= maximum_width_, null_str);

	curr_row_height_ = min_row_height_;
	curr_loc_.first = get_min_x(curr_loc_.second, curr_row_height_);
}

void tintegrate::adjust_last_row()
{
	for (std::list<titem *>::iterator it = last_row_.begin(); it != last_row_.end(); ++it) {
		titem& itm = *(*it);
		const int gap = curr_row_height_ - itm.rect.h;
		if (align_bottom_) {
			itm.rect.y += gap;
		} else {
			itm.rect.y += gap / 2;
		}
		itm.holden_rect = create_rect(itm.rect.x, curr_loc_.second, itm.rect.w, curr_row_height_);
	}
}

int tintegrate::get_remaining_width()
{
	const int total_w = get_max_x(curr_loc_.second, curr_row_height_);
	return total_w - curr_loc_.first;
}

tpoint tintegrate::get_size() const
{
	int max_x = 0;
	int max_y = 0;

	for (std::list<titem>::const_iterator it = items_.begin(), end = items_.end(); it != end; ++it) {
		SDL_Rect dst = it->rect;
		max_x = std::max<int>(max_x, dst.x + dst.w);
		max_y = std::max<int>(max_y, dst.y + dst.h);
	}

	// in order to align with hdpi_scale, both width and height returned by tintegrate must always ceil align width hdpi_scale!
	max_x = posix_align_ceil2(max_x, gui2::twidget::hdpi_scale);
	max_y = posix_align_ceil2(max_y, gui2::twidget::hdpi_scale);

	return tpoint(max_x, max_y);
}

surface tintegrate::get_surface()
{
	const tpoint size = get_size();
	surface screen = create_neutral_surface(size.x, size.y);

	int index = 0;
	for (std::list<titem>::iterator it = items_.begin(), end = items_.end(); it != end; ++ it, index ++) {
		const titem& item = *it;
		if (item.line_spacer()) {
			continue;
		}
		SDL_Rect dst = it->rect;
		if (item.box) {
			for (int i = 0; i < box_width; ++i) {
				draw_rectangle(dst.x, dst.y, item.rect.w - i * 2, item.rect.h - i * 2, 0, screen);
				++ dst.x;
				++ dst.y;
			}
		}
		
		if (!item.anim.empty()) {
			if (anims_.find(index) == anims_.end()) {
				int id = start_cycle_float_anim(item.anim);
				anims_.insert(std::make_pair(index, id));
			}

		} else {
			tsurface_blend_none_lock lock(it->surf);
			sdl_blit(it->surf, NULL, screen, &dst);
		}
	}

	return screen;
}

void tintegrate::animated_undraw(texture& canvas, const SDL_Rect& clip_rect)
{
	if (!exist_anim_) {
		return;
	}

	for (std::map<int, int>::const_reverse_iterator rit = anims_.rbegin(); rit != anims_.rend(); ++ rit) {
		std::list<titem>::const_iterator choice = items_.begin();
		std::advance(choice, rit->first);
		const titem& item = *choice;
		SDL_Rect rect = item.rect;
		rect.x += layout_offset_.x;
		rect.y += layout_offset_.y;
		if (!rects_overlap(rect, clip_rect)) {
			continue;
		}

		float_animation& anim = *dynamic_cast<float_animation*>(&anim2::manager::instance->area_anim(rit->second));
		anim.undraw(canvas);
	}
}

void tintegrate::animated_draw(texture& canvas, const SDL_Rect& clip_rect)
{
	if (!exist_anim_) {
		return;
	}
	for (std::map<int, int>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
		std::list<titem>::const_iterator choice = items_.begin();
		std::advance(choice, it->first);
		const titem& item = *choice;
		SDL_Rect rect = item.rect;
		rect.x += layout_offset_.x;
		rect.y += layout_offset_.y;
		if (!rects_overlap(rect, clip_rect)) {
			continue;
		}
		draw_canvas_anim(it->second, canvas, rect, true);
	}
}

void tintegrate::fill_locator_rect(std::vector<tlocator>& locator, bool use_max_width)
{
	std::list<titem>::const_iterator item_it = items_.begin();
	const titem* start = NULL;
	const titem* last = NULL;

	int max_x = 0;
	size_t index = 0;
	for (std::vector<tlocator>::iterator it = locator.begin(); it != locator.end(); ++ it) {
		tlocator& l = *it;
		int min_width = l.cfg["min_width"].to_int();
		int min_height = l.cfg["min_height"].to_int();
		for (; item_it != items_.end(); ++ item_it, index ++) {
			const titem& item = *item_it;
			if (item.markup_pos == l.start) {
				start = &item;

			} else if (item.markup_pos == l.end) {
				l.rect = create_rect(start->holden_rect.x, start->holden_rect.y, max_x - start->holden_rect.x, last->holden_rect.y + last->holden_rect.h - start->holden_rect.y);
				max_x = 0;
				break;

			}

			last = &item;
			max_x = std::max<int>(max_x, item.holden_rect.x + item.holden_rect.w);
			if (index == items_.size() - 1) {
				l.rect = create_rect(start->holden_rect.x, start->holden_rect.y, max_x - start->holden_rect.x, item.holden_rect.y + item.holden_rect.h - start->holden_rect.y);
				break;
			}
		}
		if (use_max_width) {
			l.rect.x = 0;
			l.rect.w = maximum_width_;
		}
		if (l.rect.w < min_width) {
			l.rect.w = min_width;
		}
		if (l.rect.h < min_height) {
			l.rect.h = min_height;
		}
		gui2::tcanvas::parse_cfg(l.cfg, l.shapes);
	}
/*
	if (!locator.empty()) {
		exist_anim_ = true;
	}
*/
	bubble_anims_ = locator;
}

void tintegrate::draw_bubble_shape(surface& canvas, game_logic::map_formula_callable& variables)
{
	for (std::vector<tlocator>::iterator it = bubble_anims_.begin(); it != bubble_anims_.end(); ++ it) {
		tlocator& l = *it;
		for (std::vector<gui2::tcanvas::tshape_ptr>::iterator itor = l.shapes.begin(); itor != l.shapes.end(); ++ itor) {
			if ((*itor)->type == gui2::tcanvas::anim_shape) {
				// bubble not support [anim]
				continue;
			}
			variables.add("xi", variant(l.rect.x));
			variables.add("yi", variant(l.rect.y));
			variables.add("widthi", variant(l.rect.w));
			variables.add("heighti", variant(l.rect.h));
/*
			int ii = 0;
			(*itor)->draw(canvas, variables);
*/
		}
	}
}

bool tintegrate::item_at::operator()(const titem& item) const 
{
	if (!item.rect.w) {
		return false;
	}
	return point_in_rect(x_, y_, item.rect);
}

std::string tintegrate::ref_at(const int x, const int y)
{
	if (x > 0 && y > 0) {
		const std::list<titem>::const_iterator it =
			std::find_if(items_.begin(), items_.end(), item_at(x, y));
		if (it != items_.end()) {
			if ((*it).ref_to != "") {
				return ((*it).ref_to);
			}
		}
	}
	return "";
}

SDL_Rect tintegrate::holden_rect(int x, int y) const
{
	if (items_.empty()) {
		return create_rect(0, 0, 0, 0);
	}
	SDL_Rect ret, last_rect;
	last_rect.y = -1;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& n = *it;
		ret = n.holden_rect;
		if (point_in_rect(x, y, n.holden_rect)) {
			ret.x = x;
			return ret;
		}
		if (last_rect.y != -1 && n.holden_rect.y != last_rect.y) {
			if (y < n.holden_rect.y) {
				last_rect.x += last_rect.w;
				return last_rect;
			}
		}
		last_rect = n.holden_rect;
	}

	// at end, return last item.
	ret.x += ret.w;
	return ret;
}

tintegrate::tloc_result tintegrate::location_from_pixel(int x, int y, bool fail_to_back) const
{
	if (items_.empty() || x < 0 || y < 0) {
		return tloc_result();
	}

	int index = 0;
	int last_y = -1;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it, index ++) {
		const titem& n = *it;
		if (point_in_rect(x, y, n.holden_rect)) {
			return tloc_result(index);
		}
		if (last_y != -1 && n.holden_rect.y != last_y) {
			if (y < n.holden_rect.y) {
				return tloc_result(index - 1);
			}
		}
		last_y = n.holden_rect.y;
	}

	if (fail_to_back) {
		return tloc_result(index - 1);
	}
	return tloc_result();
}

SDL_Rect tintegrate::editable_at(const int x, const int y) const
{
	SDL_Rect ret;
	ret.x = ret.y = 0;
	if (!editable_ || x < 0 || y < 0 || items_.empty()) {
		return create_rect(0, 0, 0, 0);
	}

	tloc_result loc = location_from_pixel(x, y, false);
	if (loc.index == -1) {
		const titem& n = items_.back();
		ret = n.holden_rect;
		ret.x += ret.w;
		return ret;
	}
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	const titem& n = *loc.it;

	int max_x_offset = x - n.rect.x;
	int x_at = 0;
	std::string substr;
	if (!n.text_type()) {
		if (max_x_offset >= n.rect.w) {
			x_at = n.rect.w;
		}
	} else if (!n.text.empty()) {
		// only process text
		utils::utf8_iterator it(n.text);
		for(; it != utils::utf8_iterator::end(n.text); ++it) {
			substr.append(it.substr().first, it.substr().second);
			int x_at_append = font::line_width(substr, n.font_size, n.style);

			if (x_at_append > max_x_offset) {
				break;
			}
			x_at = x_at_append;
		}
	}
	ret = n.holden_rect;
	ret.x += x_at;
	return ret;
}

surface tintegrate::magnifier_surf(const SDL_Rect& cursor_rect, int& cursor_x_offset) const
{
	if (!editable_) {
		return surface();
	}
	VALIDATE(items_.size(), null_str);

	surface ret;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const titem& n = *it;
		if (cursor_rect.y != n.holden_rect.y || cursor_rect.h != n.holden_rect.h || cursor_rect.w != n.holden_rect.w) {
			continue;
		}
		if (cursor_rect.x < n.holden_rect.x) {
			continue;
			
		}
		
		const int throshold_chars = 3;
		std::list<std::string> chars;
		int max_x_offset = cursor_rect.x - n.rect.x;
		bool found = max_x_offset? false: true;
		std::string substr;
		int prefix_chars = 0, post_chars = 0;
		if (!n.text_type()) {
			
		} else if (!n.text.empty()) {
			// only process text
			utils::utf8_iterator it(n.text);
			for(; it != utils::utf8_iterator::end(n.text); ++it) {
				const std::string ch(it.substr().first, it.substr().second);
				substr.append(ch);
				int x_at_append = font::line_width(substr, n.font_size, n.style);

				chars.push_back(ch);
				if (!found) {
					if (chars.size() > throshold_chars) {
						chars.pop_front();
					}
				} else {
					post_chars ++;
					if (post_chars == throshold_chars) {
						break;
					}
				}
				if (!found && x_at_append >= max_x_offset) {
					found = true;
					prefix_chars = chars.size();
				}
			}

			std::string ss;
			for (std::list<std::string>::const_iterator it = chars.begin(); it != chars.end(); ++ it) {
				ss += *it;
			}
			if (!post_chars) {
				// it is at end, append " ".
				ss += " ";
			}

			const int magnifier_font_size = font::SIZE_LARGE;
			substr.clear();
			it = utils::utf8_iterator::begin(ss);
			for (; prefix_chars && it != utils::utf8_iterator::end(ss); ++it, prefix_chars --) {
				const std::string ch(it.substr().first, it.substr().second);
				substr.append(ch);
			}
			cursor_x_offset = font::line_width(substr, magnifier_font_size, n.style);

			ret = font::get_rendered_text(ss, 0, magnifier_font_size, uint32_to_color(0xff000000), false);
			for (int i = 0; i < 2 * gui2::twidget::hdpi_scale; i ++) {
				draw_line(ret, 0xff0000ff, cursor_x_offset + i, 0, cursor_x_offset + i, ret->h);
			}
		}
		break;
	}

	return ret;
}

int tintegrate::titem::src_end_is_lf(const std::string& src) const
{
	if (text_type()) {
		if (src[pos + src_size - 1] == '\n') {
			return 1;
		}
	} else if ((int)src.size() > pos + src_size) {
		if (src[pos + src_size] == '\n') {
			return 1;
		}
	}
	return 0;
}

SDL_Rect tintegrate::editable_at2(int pos) const
{
	SDL_Rect ret;
	if (!editable_ || src_.empty()) {
		return create_rect(0, 0, 0, 0);
	}
	std::list<titem>::const_iterator choice_it = items_.begin();
	int tmp_pos = 0;
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); choice_it = it, ++ it) {
		const titem& item = *it;
		if (pos < item.pos) {
			break;
		}
	}

	const titem& item = *choice_it;
	// int lf_len = item.src_end_is_lf(src_);
	// if (item.pos + (int)item.text.size() + lf_len > pos) {
	int xoffset = 0;
	if (item.pos != pos) {
		if (item.text_type()) {
			std::string substr;
			utils::utf8_iterator it(item.text);
			size_t count = pos - item.pos;
			for (; count && it != utils::utf8_iterator::end(item.text); ++ it, count --) {
				substr.append(it.substr().first, it.substr().second);
			}
			xoffset = font::line_width(substr, item.font_size, item.style);
		} else {
			xoffset = item.rect.w;
		}
	}
	ret = item.holden_rect;
	ret.x += xoffset;

	return ret;
}

// @x, y must be cursor coordindate. cannot mouse pixel.
bool tintegrate::at_end(int x, int y) const
{
	if (!editable_ || src_.empty()) {
		return true;
	}
	const titem& item = items_.back();
	int end_x = item.holden_rect.x + item.holden_rect.w;
	
	return x >= end_x && y >= item.holden_rect.y;
}

std::string tintegrate::before_str(int x, int y) const
{
	std::string ret;
	if (!editable_ || x < 0 || y < 0 || items_.empty()) {
		return ret;
	}

	tloc_result loc = location_from_pixel(x, y, true);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	if (loc.it != items_.begin()) {
		for (std::list<titem>::const_iterator it = items_.begin(); it != loc.it; ++ it) {
			const titem& item = *it;
			if (!item.text_type()) {
				continue;
			}
			ret.append(item.text);
			if (item.src_end_is_lf(src_)) {
				ret.append("\n");
			}
		}
	}

	const titem& n = *loc.it;
	int max_x_offset = x - n.rect.x;
	size_t before_len = 0;
	std::string substr;
	if (!n.text.empty()) {
		// only process text
		utils::utf8_iterator it(n.text);
		for(; it != utils::utf8_iterator::end(n.text); ++it) {
			substr.append(it.substr().first, it.substr().second);
			int x_at_append = font::line_width(substr, n.font_size, n.style);

			if (x_at_append > max_x_offset) {
				break;
			}
			before_len += it.substr().second - it.substr().first;
		}
	}

	substr.erase(before_len);
	if (before_len) {
		ret.append(substr);
	}
	return ret;
}

int tintegrate::get_src_text_size(int pos, const std::string& text, bool quote_require_escape) const
{
	if (!editable_) {
		return 0;
	}

	const char escape_char = '\\';
	bool last_char_escape = false;
	int offset_in_src = pos;
	const char* c_str = src_.c_str();
	size_t end = text.size();
	for (size_t i = 0; i < end; i ++, offset_in_src ++) {
		const char c = c_str[offset_in_src];
		if (quote_require_escape && c == escape_char) {
			offset_in_src ++;
		}
	}
	if ((int)src_.size() > offset_in_src && src_[offset_in_src] == '\n') {
		offset_in_src ++;
	}
	
	return offset_in_src - pos;
}

// start, size: belown to src_
// text: text filed in titem.
void tintegrate::validate_str(int start, int size, const std::string& text, bool quote_require_escape) const
{
	std::string calculated_text;
	calculated_text.resize(size);
	const char* c_str = src_.c_str();
	const char escape_char = '\\';
	bool last_char_escape = false;
	int offset = 0;
	for (int i = start; i < start + size; i ++, offset ++) {
		char c = c_str[i];
		if (quote_require_escape && c == escape_char) {
			c = c_str[++ i];
		}
		calculated_text[offset] = c;
	}
	calculated_text.resize(offset);

	VALIDATE(calculated_text == text, "tintegrate error");
}

int tintegrate::get_src_text_size2(int pos, const std::string& text, bool quote_require_escape) const
{
	const char escape_char = '\\';
	bool last_char_escape = false;
	int offset_in_src = pos;
	const char* c_str = src_.c_str();
	size_t end = text.size();
	for (size_t i = 0; i < end; i ++, offset_in_src ++) {
		const char c = c_str[offset_in_src];
		if (quote_require_escape && c == escape_char) {
			offset_in_src ++;
		}
	}
	return offset_in_src - pos;
}

// @offset: offset in one item. titem view
char tintegrate::titem::src_char_from_item_offset(const std::string& src, int offset) const
{
	char c;
	bool last_char_escape = false;
	const char escape_char = '\\';
	int offset_in_src = pos;
	const char* c_str = src.c_str();
	for (int i = 0; i <= offset; i ++, offset_in_src ++) {
		c = c_str[offset_in_src];
		if (c == escape_char) {
			last_char_escape = true;
			c = c_str[++ offset_in_src];
		}
	}
	return c;
}

// @offset: offset in one item. titem view
int tintegrate::titem::offset_from_item_offset(const std::string& src, int offset, bool oft_is_idx) const
{
	const char escape_char = '\\';
	int adjust_offset = offset;
	if (offset) {
		const char* c_str = src.c_str();
		int src_end = src.size();
		int offset_in_src = pos;
		int end = oft_is_idx? offset + 1: offset;
		int escape_char_offset = -2;
		for (int i = 0; i < end && offset_in_src < src_end; i ++, offset_in_src ++) {
			const char c = c_str[offset_in_src];
			if (quote_require_escape && c == escape_char) {
				escape_char_offset = offset_in_src;
				offset_in_src ++;
			}
		}
		if (oft_is_idx) {
			offset_in_src --;
			if (offset_in_src - escape_char_offset == 1) {
				offset_in_src = escape_char_offset;
			}
		}
		adjust_offset = offset_in_src - pos;
	}
	return adjust_offset;
}


// @from, to: last field is on titem view.
// result doesn't include the character "to" pointer.
std::string tintegrate::substr_from_src(int from, int to) const
{
	const bool from_zero = !from;
	if (from >= (int)src_.size()) {
		return null_str;
	}
	std::string ret;
	const char escape_char = '\\';
	const tintegrate::titem* from_choice = NULL;
	const tintegrate::titem* to_choice = NULL;

	for (std::list<tintegrate::titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const tintegrate::titem& item = *it;
		if (!from_choice && item.pos + item.src_size > from) {
			if (from < item.pos) {
				// from maybe in <format> or </format> of markup. require to goto next titem's pos.
				from = item.pos;
			}
			from_choice = &item;
		}
		if (item.pos + item.src_size > to) {
			to_choice = &item;
			break;
		}
	}
	if (from > to) {
		return null_str;
	}
	if (!from_choice) {
		from_choice = &items_.back();
	}
	if (from_choice->text_type()) {
		if (from >= from_choice->pos + (int)from_choice->text.size() + from_choice->src_end_is_lf(src_)) {
			return null_str;
		}
	} else {
		if (from != from_choice->pos) {
			return null_str;
		}
	}
	if (!to_choice) {
		to_choice = &items_.back();
	}
	if (to < 1048576 && !to_choice->text_type() && (to != to_choice->pos && to != to_choice->pos + to_choice->src_size)) {
		return null_str;
	}
	int adjust_from = from;
	if (from_choice->text_type()) {
		adjust_from = from_choice->pos + from_choice->offset_from_item_offset(src_, from - from_choice->pos, true);
	}
	int adjust_to = to_choice->pos + to_choice->offset_from_item_offset(src_, to - to_choice->pos, false);

	if (from_zero && from_choice->quote_require_escape) {
		// caller incidate from = 0, and first titem is makerup.
		adjust_from = 0;
	}

	ret = src_.substr(adjust_from, adjust_to - adjust_from);
	
	return ret;
}

int escape_count(const std::string& str)
{
	const char escape_char = '\\';
	int escapes = 0;
	const char* c_str = str.c_str();
	size_t size = str.size();

	for (size_t i = 0; i < size; i ++) {
		if (c_str[i] == escape_char) {
			escapes ++;
			i ++;
		}
	}
	return escapes;
}

std::string tintegrate::handle_selection(int startx, int starty, int endx, int endy, SDL_Rect* new_pt) const
{
	std::string ret;
	if (new_pt) {
		*new_pt = create_rect(endx, endy, 0, 0);
	}

	if (!editable_) {
		return src_;
	}

	tloc_result start_loc = location_from_pixel(startx, starty, false);
	start_loc.it = items_.begin();
	std::advance(start_loc.it, start_loc.index);

	tloc_result end_loc = location_from_pixel(endx, endy, true);
	end_loc.it = items_.begin();
	std::advance(end_loc.it, end_loc.index);

	int max_x_offset = startx - start_loc.it->rect.x;
	std::string start_substr;
	size_t start_tmp_pos = start_loc.it->pos;
	if (start_loc.it->text_type()) {
		for (utils::utf8_iterator it(start_loc.it->text); it != utils::utf8_iterator::end(start_loc.it->text); ++ it) {
			std::string tmp = start_substr;
			tmp.append(it.substr().first, it.substr().second);
			if (font::line_width(tmp, start_loc.it->font_size, start_loc.it->style) > max_x_offset) {
				break;
			}
			start_substr = tmp;
		}

		if (!start_substr.empty()) {
			start_tmp_pos += start_substr.size();
		}
	} else {
		start_tmp_pos = start_loc.it->markup_pos;
	}

	// find end
	size_t end_tmp_pos = end_loc.it->pos;
	std::string end_substr;
	max_x_offset = endx - end_loc.it->rect.x;
	if (end_loc.it->text_type()) {
		utils::utf8_iterator it(end_loc.it->text);
		for (; it != utils::utf8_iterator::end(end_loc.it->text); ++ it) {
			std::string tmp = end_substr;
			tmp.append(it.substr().first, it.substr().second);
			if (font::line_width(tmp, end_loc.it->font_size, end_loc.it->style) > max_x_offset) {
				break;
			}
			end_substr = tmp;
		}

		if (new_pt && end_loc.index == (int)items_.size() - 1 && it == utils::utf8_iterator::end(end_loc.it->text)) {
			// if cursor at end of src_, must delete the last character '\n' (if exist) together.
			if (src_[end_loc.it->pos + end_loc.it->src_size - 1] == '\n') {
				end_substr += '\n';
			}
		}

		if (!end_substr.empty()) {
			end_tmp_pos += end_substr.size();
		}
	} else if (!max_x_offset) {
		end_tmp_pos = end_loc.it->markup_pos;

	} else {
		end_tmp_pos = calculate_next_markup_pos(end_loc.it);
	}

	if (new_pt) {
		// delete string in selection
		if (start_loc.it->text_type()) {
			ret = substr_from_src(0, start_tmp_pos);
		} else {
			ret = src_.substr(0, start_tmp_pos);
		}

		if (end_loc.it->text_type()) {
			ret.append(substr_from_src(end_tmp_pos));
		} else {
			ret.append(src_.substr(end_tmp_pos));
		}

		if (start_loc.it->text_type()) {
			start_tmp_pos = start_loc.it->pos + stuff_escape(start_substr, start_loc.it->quote_require_escape).size();
		} else {
			// start_tmp_pos = start_loc.it->pos;
		}
		tintegrate integrate(ret, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
		*new_pt = integrate.calculate_cursor(start_tmp_pos);

	} else {
		// copy string in selection
		const titem& start_item = *start_loc.it;
		if (start_loc.it != end_loc.it) {
			// (1/3) start part
			if (start_loc.it->text_type()) {
				if (start_item.pos + start_item.text.size() > start_tmp_pos) {
					ret.append(start_item.text.substr(start_tmp_pos - start_item.pos));
				}
				if (start_item.src_end_is_lf(src_)) {
					ret.append("\n");
				}
			} else {
				int next_markup_pos = calculate_next_markup_pos(start_loc.it);
				ret.append(src_.substr(start_tmp_pos, next_markup_pos - start_tmp_pos));
			}

			// (2/3) middle part
			std::list<titem>::const_iterator it = start_loc.it;
			for (++ it; it != end_loc.it; ++ it) {
				const titem& item = *it;
				if (item.text_type()) {
					ret.append(item.text);
					if (item.src_end_is_lf(src_)) {
						ret.append("\n");
					}
				} else {
					int next_markup_pos = calculate_next_markup_pos(it);
					ret.append(src_.substr(it->markup_pos, next_markup_pos - it->markup_pos));
				}
			}

			// (3/3) end part
			const titem& end_item = *end_loc.it;
			if (end_item.text_type()) {
				if (end_item.pos != end_tmp_pos) {
					ret.append(end_item.text.substr(0, end_tmp_pos - end_item.pos));
				}
				if (end_item.pos + end_item.text.size() < end_tmp_pos) {
					if (end_item.src_end_is_lf(src_)) {
						ret.append("\n");
					}
				}
			} else {
				if (end_tmp_pos != end_item.markup_pos) {
					VALIDATE((int)end_tmp_pos > end_item.markup_pos, null_str);

					ret.append(src_.substr(end_item.markup_pos, end_tmp_pos - end_item.markup_pos));
				}
			}
		} else {
			ret.append(start_item.text.substr(start_tmp_pos - start_item.pos, end_tmp_pos - start_tmp_pos));
		}
	}
	if (new_pt && !new_pt->h) {
		*new_pt = holden_rect(new_pt->x, new_pt->y);
	}
	return ret;
}

std::string tintegrate::handle_char(bool del, int x, int y, const bool backspace, SDL_Rect& new_pt) const
{
	std::string ret;

	new_pt = create_rect(0, 0, 0, 0);
	if (!editable_ || items_.empty() || x < 0 || y < 0) {
		return src_;
	}

	if (backspace && !x && !y) {
		new_pt = holden_rect(x, y);
		return src_;
	}
	
	tloc_result loc = location_from_pixel(x, y, false);
	if (loc.index == -1) {
		if (!backspace) {
			// case1.1(backspace): cursor at end.
			if (src_.size() == 1 || src_[0] == '\n') {
				// if only one char, and it is lf, clear this string.
				new_pt = create_rect(0, 0, 0, 0);
				return null_str;

			} else if (!items_.back().src_end_is_lf(src_)) {
				// last char isn't lf.
				new_pt = holden_rect(x, y);
				return src_;
			}
		}
		loc.index = items_.size() - 1;

	} else if (items_.size() == 1 && !backspace) {
		const titem& front = items_.front();
		if (!front.text_type()) {
			new_pt = create_rect(0, 0, 0, 0);
			return null_str;
		}
	}
	VALIDATE(loc.index != -1, null_str);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	int max_x_offset = x - loc.it->rect.x;
	if (!max_x_offset && backspace) {
		// case1.2(backspace): cursor at begin of row.
		if (!loc.index) {
			new_pt = holden_rect(x, y);
			return src_;
		}
		tloc_result before_loc(loc.index - 1);
		before_loc.it = items_.begin();
		std::advance(before_loc.it, before_loc.index);

		if (del) {
			size_t tmp_pos = before_loc.it->pos;
			size_t src_tmp_pos = tmp_pos;
			const bool before_is_markup = before_loc.it->quote_require_escape;
			if (before_loc.it->text_type()) {
				size_t last_src_char_len = 0;
				if (before_loc.it->src_end_is_lf(src_)) {
					tmp_pos += before_loc.it->text.size();
					src_tmp_pos += stuff_escape(before_loc.it->text, before_loc.it->quote_require_escape).size();

					last_src_char_len = loc.it->pos - src_tmp_pos;
					// VALIDATE(last_src_char_len == 1 || before_is_markup, null_str); // 1 is \n

				} else {
					size_t last_char_len = 0;
					for (utils::utf8_iterator it(before_loc.it->text); it != utils::utf8_iterator::end(before_loc.it->text); ++ it) {
						last_char_len = it.substr().second - it.substr().first;
						tmp_pos += last_char_len;

						last_src_char_len = last_char_len + (before_loc.it->quote_require_escape && is_require_escape_unicode(*it)? 1: 0);
						src_tmp_pos += last_src_char_len;
					}
					tmp_pos -= last_char_len;
					src_tmp_pos -= last_src_char_len;
				}

				ret = substr_from_src(0, tmp_pos);
				// I'm sure appended string from next line.
				if (loc.it->text_type()) {
					if (before_is_markup) {
						VALIDATE((int)(src_tmp_pos + last_src_char_len) <= loc.it->pos, null_str);
						if (before_loc.it->markup_pos != before_loc.it->pos && tmp_pos == before_loc.it->pos) {
							VALIDATE(tmp_pos == src_tmp_pos, null_str);
							// this markup has no text.
							// requrie erase: <format>text='
							ret.erase(before_loc.it->markup_pos, tmp_pos - before_loc.it->markup_pos);
							tmp_pos = before_loc.it->markup_pos;
							src_tmp_pos = tmp_pos;
						} else if (loc.it->pos > (int)(src_tmp_pos + last_src_char_len)) {
							// this markup's last titem.
							// require copy: ' color='red'</format>
							ret.append(src_.substr(src_tmp_pos + last_src_char_len, loc.it->pos - src_tmp_pos - last_src_char_len));
						}
					}
					if (loc.it->quote_require_escape) {
						// below substr_from_src(loc.it->pos) doesn't include <format>text='
						ret.append(src_.substr(loc.it->markup_pos, loc.it->pos - loc.it->markup_pos));
						if (src_tmp_pos == 0) {
							src_tmp_pos = loc.it->pos - last_src_char_len;
						}
					}
					ret.append(substr_from_src(loc.it->pos));
				} else {
					ret.append(src_.substr(loc.it->markup_pos));
				}

			} else {
				if (atom_markup_) {
					for (int at = before_loc.index; at >= 0; at --) {
						const titem& item = *before_loc.it;
						if (!item.index) {
							break;
						}
						VALIDATE(before_loc.index > 0, null_str);
						before_loc.index --;
						before_loc.it = items_.begin();
						std::advance(before_loc.it, before_loc.index);
					}
				}
				tmp_pos = before_loc.it->markup_pos;
				
				ret = src_.substr(0, tmp_pos);
				// loc maybe non-text_type titem.
				ret.append(src_.substr(loc.it->markup_pos));

				src_tmp_pos = tmp_pos;
			}

			tintegrate integrate(ret, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
			new_pt = integrate.calculate_cursor(src_tmp_pos);

		} else if (before_loc.it->text_type()) {
			if (loc.it->rect.y >= before_loc.it->rect.y + before_loc.it->rect.h && before_loc.it->src_end_is_lf(src_)) {
				// before below line, has \n
				new_pt.x = before_loc.it->rect.x + before_loc.it->rect.w;
			} else {
				new_pt.x = before_loc.it->rect.x + font::line_minus1_width(before_loc.it->text, before_loc.it->font_size, before_loc.it->style);
			}
			new_pt.y = before_loc.it->rect.y;

		} else {
			if (atom_markup_) {
				for (int at = before_loc.index; at >= 0; at --) {
					const titem& item = *before_loc.it;
					if (!item.index) {
						break;
					}
					VALIDATE(before_loc.index > 0, null_str);
					before_loc.index --;
					before_loc.it = items_.begin();
					std::advance(before_loc.it, before_loc.index);
				}
			}
			new_pt.x = before_loc.it->rect.x;
			new_pt.y = before_loc.it->rect.y;
		}

		if (!new_pt.h) {
			new_pt = holden_rect(new_pt.x, new_pt.y);
		}
		return ret;
	} 

	size_t tmp_pos = loc.it->pos;
	if (backspace) {
		std::string substr;
		// case1.2(backspace): cursor at intermiate of row.
		size_t last_char_len = 0;
		if (loc.it->text_type()) {
			utils::utf8_iterator it(loc.it->text);
			for (; it != utils::utf8_iterator::end(loc.it->text); ++ it) {
				std::string tmp = substr;
				tmp.append(it.substr().first, it.substr().second);
				if (font::line_width(tmp, loc.it->font_size, loc.it->style) > max_x_offset) {
					break;
				}

				last_char_len = it.substr().second - it.substr().first;
				tmp_pos += last_char_len;

				substr = tmp;
			}
		}
		if (del) {
			tmp_pos -= last_char_len;
			
			if (loc.it->text_type()) {
				ret = substr_from_src(0, tmp_pos);

				bool loc_will_delete = false;
				if (last_char_len == loc.it->text.size() && !loc.it->src_end_is_lf(src_)) {
					VALIDATE(tmp_pos == loc.it->pos, null_str);
					loc_will_delete = true;
					if (loc.it->pos != loc.it->markup_pos) {
						ret.erase(loc.it->markup_pos, loc.it->pos - loc.it->markup_pos);
					}
				}

				if (substr.size() != loc.it->text.size() || loc.it->src_end_is_lf(src_)) {
					// VALIDATE(substr.size() < loc.it->text.size(), null_str);
					ret.append(substr_from_src(tmp_pos + last_char_len));

				} else {
					std::list<titem>::const_iterator next_it = loc.it;
					++ next_it;
					if (next_it != items_.end()) {
						if (loc_will_delete) {
							ret.append(src_.substr(next_it->markup_pos));
						} else if (next_it->text_type()) {
							ret.append(substr_from_src(tmp_pos + last_char_len));
						} else {
							ret.append(src_.substr(tmp_pos + last_char_len));
						}

					} else if (!loc_will_delete && loc.it->quote_require_escape) {
						ret.append(src_.substr(tmp_pos + last_char_len));
					}
				}

				// calculate tmp_pos
				if (loc_will_delete) {
					tmp_pos = loc.it->markup_pos;
				} else {
					substr.erase(substr.size() - last_char_len);
					tmp_pos = loc.it->pos + stuff_escape(substr, loc.it->quote_require_escape).size();
				}

			} else {
				tloc_result new_loc = loc;
				if (atom_markup_) {
					for (int at = new_loc.index; at >= 0; at --) {
						const titem& item = *new_loc.it;
						if (!item.index) {
							break;
						}
						VALIDATE(new_loc.index > 0, null_str);
						new_loc.index --;
						new_loc.it = items_.begin();
						std::advance(new_loc.it, new_loc.index);
					}
				}
				ret = src_.substr(0, new_loc.it->markup_pos);

				int next_markup_pos = calculate_next_markup_pos(loc.it);
				ret.append(src_.substr(next_markup_pos));

				tmp_pos = new_loc.it->markup_pos;
			}

			tintegrate integrate(ret, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
			new_pt = integrate.calculate_cursor(tmp_pos);

		} else {
			if (loc.it->text_type()) {
				new_pt.x = loc.it->rect.x + font::line_width(substr.substr(0, substr.size() - last_char_len), loc.it->font_size, loc.it->style);
			} else {
				if (atom_markup_) {
					for (int at = loc.index; at >= 0; at --) {
						const titem& item = *loc.it;
						if (!item.index) {
							break;
						}
						VALIDATE(loc.index > 0, null_str);
						loc.index --;
						loc.it = items_.begin();
						std::advance(loc.it, loc.index);
					}
				}
				new_pt.x = loc.it->rect.x;
			}
			new_pt.y = loc.it->rect.y;
		}

		if (!new_pt.h) {
			new_pt = holden_rect(new_pt.x, new_pt.y);
		}
		return ret;
	}

	//
	// handle delete/right logic.
	//
	std::string substr;
	for (utils::utf8_iterator it(loc.it->text); it != utils::utf8_iterator::end(loc.it->text); ++ it) {
		std::string tmp = substr;
		tmp.append(it.substr().first, it.substr().second);
		if (font::line_width(tmp, loc.it->font_size, loc.it->style) > max_x_offset) {
			break;
		}
		substr = tmp;
	}

	int end_len = del? loc.it->src_end_is_lf(src_): 0;
	if (del) {
		tmp_pos += (int)substr.size();

		int src_pos = loc.it->pos + stuff_escape(substr, loc.it->quote_require_escape).size();
		bool src_pos_adjusted = false;

		if (loc.it->text_type()) {
			if (loc.it->text.size() + end_len == substr.size()) {
				// case2.1(delete): cursor at end of row. i think it cannnot happen.
				std::list<titem>::const_iterator before_it = loc.it;
				++ loc.it;
				tmp_pos = loc.it->pos;
			}

			ret = substr_from_src(0, tmp_pos);

			std::string::const_iterator it = src_.begin();
			std::advance(it, tmp_pos);
			utils::utf8_iterator it2(it, src_.end());
			const wchar_t last_ch = *it2;

			std::list<titem>::const_iterator next_it = loc.it;
			++ next_it;
			const size_t last_char_len = std::distance(it2.substr().first, it2.substr().second);

			size_t last_src_char_len = last_char_len;
			if (loc.it->quote_require_escape && is_require_escape_unicode(last_ch)) {
				last_src_char_len ++;
			}

			tmp_pos += last_char_len;
			if (next_it == items_.end() || next_it->text_type()) {
				const int next_markup_pos = next_it == items_.end()? src_.size(): next_it->markup_pos;
				if (loc.it->quote_require_escape) {
					if (loc.it->markup_pos != loc.it->pos && loc.it->src_size == last_src_char_len && next_markup_pos != loc.it->pos + last_src_char_len) {
						// this markup's will be empty.
						VALIDATE(tmp_pos == loc.it->pos + last_char_len, null_str);
						ret.erase(loc.it->markup_pos, tmp_pos - last_char_len - loc.it->markup_pos);

						src_pos = loc.it->markup_pos;
						tmp_pos = next_markup_pos;

						src_pos_adjusted = true;

					} else if (next_markup_pos > (int)(src_pos + last_src_char_len) && tmp_pos == loc.it->pos + loc.it->text.size() + loc.it->src_end_is_lf(src_)) {
						// this markup's last titem.
						// require copy: ' color='red'</format>
						ret.append(src_.substr(src_pos + last_src_char_len, next_markup_pos - src_pos - last_src_char_len));
						tmp_pos = next_markup_pos;
					}
				}
				if (next_it != items_.end() && next_it->quote_require_escape && tmp_pos == loc.it->pos + loc.it->text.size()) {
					// below substr_from_src(tmp_pos) doesn't include <format>text='
					ret.append(src_.substr(next_it->markup_pos, next_it->pos - next_it->markup_pos));
				}
				ret.append(substr_from_src(tmp_pos));
			} else {
				ret.append(src_.substr(next_it->markup_pos));
			}

			if (!src_pos_adjusted) {
				src_pos = loc.it->pos + stuff_escape(substr, loc.it->quote_require_escape).size();
			}

		} else {
			std::list<titem>::const_iterator next_it = loc.it;
			++ next_it;

			if (atom_markup_) {
				for (; next_it != items_.end(); ++ next_it) {
					const titem& item = *next_it;
					if (!item.index) {
						break;
					}
				}
			}

			ret = src_.substr(0, loc.it->markup_pos);
			if (next_it != items_.end()) {
				ret.append(src_.substr(next_it->markup_pos));
			}

			src_pos = loc.it->markup_pos;
		}

		tintegrate integrate(ret, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
		new_pt = integrate.calculate_cursor(src_pos);

	} else {
		if (loc.it->text_type()) {
			std::string substr_plus1 = substr;
			bool to_next_item = false;
			if (loc.it->text.size() != substr.size()) {
				std::string::const_iterator it = loc.it->text.begin();
				if (!substr_plus1.empty()) {
					std::advance(it, substr_plus1.size());
				}
				utils::utf8_iterator it2(it, loc.it->text.end());
				substr_plus1.append(it2.substr().first, it2.substr().second);
				if (substr_plus1.size() == loc.it->text.size() && !loc.it->src_end_is_lf(src_)) {
					// this titem not \n, goto goto next_item direct.
					to_next_item = true;
				}
			}

			if (to_next_item || loc.it->text.size() == substr.size()) {
				std::list<titem>::const_iterator before_it = loc.it;
				++ loc.it;

				if (loc.it != items_.end()) {
					new_pt.x = loc.it->rect.x;
					new_pt.y = loc.it->rect.y;
				} else {
					const titem& item = items_.back();
					new_pt.x = item.rect.x + item.rect.w;
					new_pt.y = item.rect.y;
				}
			} else {
				new_pt.x = loc.it->rect.x + font::line_width(substr_plus1, loc.it->font_size, loc.it->style);
				new_pt.y = loc.it->rect.y;
			}

		} else {
			std::list<titem>::const_iterator next_it = loc.it;
			++ next_it;

			if (atom_markup_) {
				// forward until ttitem.index = 0;
				for (; next_it != items_.end(); ++ next_it) {
					const titem& item = *next_it;
					if (!item.index) {
						break;
					}
				}
			}

			if (next_it != items_.end()) {
				new_pt.x = next_it->rect.x;
				new_pt.y = next_it->rect.y;
			} else {
				const titem& item = items_.back();
				new_pt.x = item.rect.x + item.rect.w;
				new_pt.y = item.rect.y;
			}
		}
	}

	if (!new_pt.h) {
		new_pt = holden_rect(new_pt.x, new_pt.y);
	}
	return ret;
}

SDL_Rect tintegrate::next_editable_at(const SDL_Rect& start_rect) const
{
	VALIDATE(editable_ && items_.size(), null_str);

	SDL_Rect ret = empty_rect;

	tloc_result loc = location_from_pixel(start_rect.x, start_rect.y, true);
	VALIDATE(loc.index != -1, null_str);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);
	const titem& n = *loc.it;

	if (n.rect.w) {
		ret = n.rect;
		if (start_rect.x != n.rect.x + n.rect.w) {
			ret.x = n.rect.x + n.rect.w;
		}
	}

	for (int index = loc.index + 1; !ret.w && index < (int)items_.size(); index ++) {
		loc.it = items_.begin();
		std::advance(loc.it, index);
		const titem& n = *loc.it;
		if (n.rect.w) {
			ret = n.rect;
			ret.x = n.rect.x + n.rect.w;
		}
	}

	for (int index = loc.index - 1; !ret.w && index >= 0; index --) {
		loc.it = items_.begin();
		std::advance(loc.it, index);
		const titem& n = *loc.it;
		if (n.rect.w) {
			ret = n.rect;
		}
	}

	return ret;
}

int tintegrate::calculate_next_markup_pos(const std::list<titem>::const_iterator& choice_it) const
{
	std::list<titem>::const_iterator next_it = choice_it;
	++ next_it;
	return next_it == items_.end()? src_.size(): next_it->markup_pos;
}

SDL_Rect tintegrate::calculate_cursor(int src_pos) const
{
	if (items_.empty()) {
		return create_rect(0, 0, 0, 0);
	}
	std::list<titem>::const_iterator choice_it = items_.begin();
	for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); choice_it = it, ++ it) {
		const titem& item = *it;
		if ((int)src_pos < item.pos) {
			break;
		}
	}
	size_t utf8_tmp_pos;
	if (choice_it->text_type()) {
		std::string substr = src_.substr(choice_it->pos, src_pos - choice_it->pos);
		substr = drop_escape(substr, choice_it->quote_require_escape);
		utf8_tmp_pos = choice_it->pos + utils::utf8str_len(substr);
	} else {
		// choice_it has been next item.
		std::list<titem>::const_iterator next_it = choice_it;
		++ next_it;
		const int next_markup_pos = next_it == items_.end()? src_.size(): next_it->markup_pos;

		utf8_tmp_pos = choice_it->pos;
		if (src_pos == next_markup_pos) {
			utf8_tmp_pos ++;
		}
	}

	return editable_at2(utf8_tmp_pos);
}

int tintegrate::calculate_src_pos(int x, int y) const
{
	if (items_.empty()) {
		return 0;
	}
	
	tloc_result loc = location_from_pixel(x, y, true);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	int src_pos = loc.it->pos;
	std::string substr;
	int max_x_offset = x - loc.it->rect.x;
	if (loc.it->text_type()) {
		for (utils::utf8_iterator it(loc.it->text); it != utils::utf8_iterator::end(loc.it->text); ++ it) {
			std::string tmp = substr;
			tmp.append(it.substr().first, it.substr().second);
			if (font::line_width(tmp, loc.it->font_size, loc.it->style) > max_x_offset) {
				break;
			}
			substr = tmp;
		}
		src_pos += stuff_escape(substr, loc.it->quote_require_escape).size();

	} else if (max_x_offset) {
		src_pos += loc.it->src_size;

	}

	return src_pos;
}

std::string tintegrate::insert_str(int x, int y, const std::string& str, SDL_Rect& new_pt) const
{
	VALIDATE(str.find('\r') == std::string::npos, null_str);
	std::string ret;

	new_pt = create_rect(x, y, 0, 0);
	if (!editable_ || str.empty() || x < 0 || y < 0) {
		return src_;
	}

	if (src_.empty()) {
		tintegrate integrate(str, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
		// goto end
		new_pt = integrate.editable_at2(str.size());
		return str;
	}
	
	tloc_result loc = location_from_pixel(x, y, true);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	std::string substr;
	int max_x_offset = x - loc.it->rect.x;

	size_t tmp_pos = loc.it->pos;
	
	bool quote_require_escape = loc.it->quote_require_escape;
	if (loc.it->text_type()) {
		for (utils::utf8_iterator it(loc.it->text); it != utils::utf8_iterator::end(loc.it->text); ++ it) {
			std::string tmp = substr;
			tmp.append(it.substr().first, it.substr().second);
			if (font::line_width(tmp, loc.it->font_size, loc.it->style) > max_x_offset) {
				break;
			}
			substr = tmp;
		}

		if (!substr.empty()) {
			tmp_pos += substr.size();
		}
	} else if (!max_x_offset) {
		tmp_pos = loc.it->markup_pos;
		// must not insert str in non-text_type() titem.
		quote_require_escape = false;

	} else {
		std::list<titem>::const_iterator next_it = loc.it;
		++ next_it;
		const int next_markup_pos = next_it == items_.end()? src_.size(): next_it->markup_pos;
		tmp_pos = next_markup_pos;
		// must not insert str in non-text_type() titem.
		quote_require_escape = false;
	}

	if (loc.it->text_type()) {
		ret = substr_from_src(0, tmp_pos);
	} else {
		ret = src_.substr(0, tmp_pos);
	}

	std::string str2;
	if (!loc.it->text_type() || !loc.it->quote_require_escape) {
		str2 = stuff_escape(str, quote_require_escape);
	} else {
		str2 = utils::ht_drop_markup(str, support_markups, "format");
	}
	ret.append(str2);
	if (loc.it->text_type()) {
		if (!loc.it->quote_require_escape) {
			ret.append(substr_from_src(tmp_pos));
		} else {
			ret.append(src_.substr(tmp_pos));
		}
	} else {
		ret.append(src_.substr(tmp_pos));
	}

	// calculate cursor postion
	if (loc.it->text_type()) {
		tmp_pos = loc.it->pos + stuff_escape(substr, loc.it->quote_require_escape).size() + str2.size();
	} else if (!max_x_offset) {
		tmp_pos = loc.it->pos + str2.size();
	} else {
		tmp_pos += str2.size();
	}
	tintegrate integrate(ret, maximum_width_, default_font_size_, default_font_color_, true, atom_markup_);
	new_pt = integrate.calculate_cursor(tmp_pos);

	return ret;
}

SDL_Rect tintegrate::key_arrow(int x, int y, bool up) const
{
	if (items_.empty() || x < 0 || y < 0) {
		return empty_rect;
	}

	tloc_result loc = location_from_pixel(x, y, true);
	loc.it = items_.begin();
	std::advance(loc.it, loc.index);

	int new_index = loc.index;
	if (up && loc.index) {
		int index = items_.size() - 1;
		for (std::list<titem>::const_reverse_iterator it = items_.rbegin(); it != items_.rend(); ++ it, index --) {
			const titem& item = *it;
			if (item.rect.y + item.rect.h <= loc.it->rect.y) {
				new_index = index;
				break;
			}
		}
		
	} else if (!up && loc.index != items_.size() - 1) {
		int index = 0;
		for (std::list<titem>::const_iterator it = items_.begin(); it != items_.end(); ++ it, index ++) {
			const titem& item = *it;
			if (item.rect.y >= loc.it->rect.y + loc.it->rect.h) {
				new_index = index;
				break;
			}
		}

	} else {
		return empty_rect;
	}
	std::list<titem>::const_iterator new_it = items_.begin();
	std::advance(new_it, new_index);
	y = new_it->rect.y;
	return editable_at(x, y);
}

void tintegrate::clear()
{
	src_.clear();
	items_.clear();
}
