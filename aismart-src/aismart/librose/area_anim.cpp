/* $Id: unit_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

/** @file */

#include "global.hpp"
#include "area_anim.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "wml_exception.hpp"
#include "base_instance.hpp"

#include "sound.hpp"
#include <boost/foreach.hpp>

namespace anim_field_tag {

std::map<const std::string, tfield> tags;

void fill_tags()
{
	if (!tags.empty()) return;
	tags.insert(std::make_pair("x", X));
	tags.insert(std::make_pair("Y", Y));
	tags.insert(std::make_pair("offset_x", OFFSET_X));
	tags.insert(std::make_pair("offset_y", OFFSET_Y));
	tags.insert(std::make_pair("image", IMAGE));
	tags.insert(std::make_pair("image_mod", IMAGE_MOD));
	tags.insert(std::make_pair("text", TEXT));
	tags.insert(std::make_pair("alpha", ALPHA));
}

bool is_progressive_field(tfield field)
{
	return field == X || field == Y || field == OFFSET_X || field == OFFSET_Y || field == ALPHA;
}

tfield find(const std::string& tag)
{
	std::map<const std::string, tfield>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return NONE;
}

const std::string& rfind(tfield tag)
{
	for (std::map<const std::string, tfield>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

}

namespace anim2 {

manager* manager::instance = nullptr;

manager::manager(CVideo& video)
	: tdrawing_buffer(video)
{
	VALIDATE(instance == nullptr, "only allows a maximum of one anim2::manager");
	instance = this;
}

manager::~manager()
{
	instance = nullptr;
}

double manager::get_zoom_factor() const
{
	return 1;
}

SDL_Rect manager::clip_rect_commit() const
{
	return rt.rect;
}

void manager::draw_window_anim()
{
	// must not be in is_scene.
	if (area_anims_.empty()) {
		return;
	}

	new_animation_frame();

	SDL_Rect rect;
	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		animation& anim = *it->second;
		if (anim.type() != anim_window || !anim.started()) {
			continue;
		}

		anim.update_last_draw_time();

		float_animation* anim2 = dynamic_cast<float_animation*>(&anim);
		rect = anim2->special_rect();
		if (is_null_rect(rect)) {
			rect = screen_area();
		}
		anim.redraw(video_.getTexture(), rect, true);
	}

	drawing_buffer_commit(video_.getTexture(), clip_rect_commit());
}

void manager::undraw_window_anim()
{
	// must not be in is_scene.
	if (area_anims_.empty()) {
		return;
	}

	for (std::map<int, animation*>::reverse_iterator it = area_anims_.rbegin(); it != area_anims_.rend(); ++ it) {
		animation& anim = *it->second;
		if (anim.type() != anim_window || !anim.started()) {
			continue;
		}
		anim.undraw(video_.getTexture());
	}

	for (std::map<int, animation*>::iterator it = area_anims_.begin(); it != area_anims_.end(); ) {
		animation& anim = *it->second;
		if (anim.type() != anim_window || !anim.started()) {
			++ it;
			continue;
		}
		if (anim.started() && !anim.cycles() && anim.animation_finished_potential()) {
			anim.require_release();
			delete it->second;
			area_anims_.erase(it ++);
		} else {
			++ it;
		}
	}
}

std::map<const std::string, int> tags;

void fill_tags()
{
	anim_field_tag::fill_tags();

	if (!tags.empty()) return;
	tags.insert(std::make_pair("location", LOCATION));
	tags.insert(std::make_pair("hscroll_text", HSCROLL_TEXT));
	tags.insert(std::make_pair("title_screen", TITLE_SCREEN));
	tags.insert(std::make_pair("operating", OPERATING));

	instance->app_fill_anim_tags(tags);
}

int find(const std::string& tag)
{
	std::map<const std::string, int>::const_iterator it = tags.find(tag);
	if (it != tags.end()) {
		return it->second;
	}
	return nposm;
}

const std::string& rfind(int tag)
{
	for (std::map<const std::string, int>::const_iterator it = tags.begin(); it != tags.end(); ++ it) {
		if (it->second == tag) {
			return it->first;
		}
	}
	return null_str;
}

void fill_anims(const config& cfg)
{
	if (!instance) {
		// normally instance must not NULL. but now editor.exe, kingdomd.exe
		return;
	}

	fill_tags();
	instance->clear_anims();

	utils::string_map symbols;
	std::stringstream ss;

	std::set<int> constructed;

	symbols["child"] = ht::generate_format("animation", color_to_uint32(font::YELLOW_COLOR));
	BOOST_FOREACH (const config &anim, cfg.child_range("animation")) {
		const std::string& app = anim["app"].str();

		if (!app.empty() && app != game_config::app) {
			continue;
		}

		const std::string& id = anim["id"].str();
		VALIDATE(!id.empty(), vgettext2("[$child] child, $child must has id attribute!", symbols));

		symbols["id"] = ht::generate_format(id, color_to_uint32(font::BAD_COLOR));

		const config& sub = anim.child("anim");
		VALIDATE(sub, vgettext2("[$child] child, $child must has [anim] child!", symbols));

		bool area = sub["area_mode"].to_bool();
		bool tpl = !area && anim["template"].to_bool();

		int at = nposm;
		if (area || !tpl) {
			at = find(id);
			VALIDATE(at != nposm, vgettext2("[$child] child, WML defined $id, but cannot find corresponding code id!", symbols));
			VALIDATE(constructed.find(at) == constructed.end(), vgettext2("[$child] child, $id is defined repeatly!", symbols));
			constructed.insert(at);
		}

		instance->fill_anim(at, id, area, tpl, sub);
	}
}

const animation* anim(int at)
{
	return instance->anim(at);
}

static void utype_anim_replace_cfg_inernal(const config& src, config& dst, const utils::string_map& symbols)
{
	BOOST_FOREACH (const config::any_child& c, src.all_children_range()) {
		config& adding = dst.add_child(c.key);
		BOOST_FOREACH (const config::attribute &i, c.cfg.attribute_range()) {
			adding[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
		}
		utype_anim_replace_cfg_inernal(c.cfg, adding, symbols);
	}
}

void utype_anim_create_cfg(const std::string& anim_renamed_key, const std::string& tpl_id, config& dst, const utils::string_map& symbols)
{
	typedef std::multimap<std::string, const config>::const_iterator Itor;
	std::pair<Itor, Itor> its = instance->utype_anim_tpls().equal_range(tpl_id);
	while (its.first != its.second) {
		// tpl          animation instance
		// [anim] ----> [anim_renamed_key]
		config& sub_cfg = dst.add_child(anim_renamed_key);
		if (symbols.count("offset_x")) {
			sub_cfg["offset_x"] = symbols.find("offset_x")->second;
		}
		if (symbols.count("offset_y")) {
			sub_cfg["offset_y"] = symbols.find("offset_y")->second;
		}
		const config& anim_cfg = its.first->second;
		// replace
		BOOST_FOREACH (const config::attribute &i, anim_cfg.attribute_range()) {
			sub_cfg[i.first] = utils::interpolate_variables_into_string(i.second, &symbols);
		}
		utype_anim_replace_cfg_inernal(anim_cfg, sub_cfg, symbols);
		++ its.first;
	}
}

rt_instance rt;

}

static bool buf_use_texture = true;
float_animation::float_animation(const animation& anim)
	: animation(anim)
	, std_w_(0)
	, std_h_(0)
	, constrained_scale_(true)
	, up_scale_(false)
	, bufs_()
	, special_rect_(null_rect)
{
}

namespace {
struct event {
	int x, y, w, h;
	bool in;
	event(const SDL_Rect& rect, bool i) : x(i ? rect.x : rect.x + rect.w), y(rect.y), w(rect.w), h(rect.h), in(i) { }
};
bool operator<(const event& a, const event& b) {
	if (a.x != b.x) return a.x < b.x;
	if (a.in != b.in) return a.in;
	if (a.y != b.y) return a.y < b.y;
	if (a.h != b.h) return a.h < b.h;
	if (a.w != b.w) return a.w < b.w;
	return false;
}
bool operator==(const event& a, const event& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.in == b.in;
}

struct segment {
	int x, count;
	segment() : x(0), count(0) { }
	segment(int x, int count) : x(x), count(count) { }
};


std::vector<SDL_Rect> update_rects;
std::vector<event> events;
std::map<int, segment> segments;

void calc_rects()
{
	events.clear();

	BOOST_FOREACH(SDL_Rect const &rect, update_rects) {
		events.push_back(event(rect, true));
		events.push_back(event(rect, false));
	}

	std::sort(events.begin(), events.end());
	std::vector<event>::iterator events_end = std::unique(events.begin(), events.end());

	segments.clear();
	update_rects.clear();

	for (std::vector<event>::iterator iter = events.begin(); iter != events_end; ++iter) {
		std::map<int, segment>::iterator lower = segments.find(iter->y);
		if (lower == segments.end()) {
			lower = segments.insert(std::make_pair(iter->y, segment())).first;
			if (lower != segments.begin()) {
				std::map<int, segment>::iterator prev = lower;
				--prev;
				lower->second = prev->second;
			}
		}

		if (lower->second.count == 0) {
			lower->second.x = iter->x;
		}

		std::map<int, segment>::iterator upper = segments.find(iter->y + iter->h);
		if (upper == segments.end()) {
			upper = segments.insert(std::make_pair(iter->y + iter->h, segment())).first;
			std::map<int, segment>::iterator prev = upper;
			--prev;
			upper->second = prev->second;
		}

		if (iter->in) {
			while (lower != upper) {
				++lower->second.count;
				++lower;
			}
		} else {
			while (lower != upper) {
				lower->second.count--;
				if (lower->second.count == 0) {
					std::map<int, segment>::iterator next = lower;
					++next;

					int x = lower->second.x, y = lower->first;
					unsigned w = iter->x - x;
					unsigned h = next->first - y;
					SDL_Rect a = create_rect(x, y, w, h);

					if (update_rects.empty()) {
						update_rects.push_back(a);
					} else {
						SDL_Rect& p = update_rects.back(), n;
						int pa = p.w * p.h, aa = w * h, s = pa + aa;
						int thresh = 51;

						n.w = std::max<int>(x + w, p.x + p.w);
						n.x = std::min<int>(p.x, x);
						n.w -= n.x;
						n.h = std::max<int>(y + h, p.y + p.h);
						n.y = std::min<int>(p.y, y);
						n.h -= n.y;

						if (s * 100 < thresh * n.w * n.h) {
							update_rects.push_back(a);
						} else {
							p = n;
						}
					}

					if (lower == segments.begin()) {
						segments.erase(lower);
					} else {
						std::map<int, segment>::iterator prev = lower;
						--prev;
						if (prev->second.count == 0) segments.erase(lower);
					}

					lower = next;
				} else {
					++lower;
				}
			}
		}
	}
}

}

bool float_animation::invalidate(const int fb_width, const int fb_height, frame_parameters& value)
{
	value.primary_frame = t_true;
	std::vector<SDL_Rect> rects = unit_anim_.get_overlaped_rect(value, src_, dst_);
	value.primary_frame = t_false;
	for (std::map<std::string, particular>::iterator anim_itor = sub_anims_.begin(); anim_itor != sub_anims_.end() ; ++ anim_itor) {
		std::vector<SDL_Rect> tmp = anim_itor->second.get_overlaped_rect(value, src_, dst_);
		rects.insert(rects.end(), tmp.begin(), tmp.end());
	}
/*
	update_rects.clear();

	for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		SDL_Rect rect = *it;
		if (rect.x < 0) {
			if (rect.x * -1 >= int(rect.w)) {
				continue;
			}
			rect.w += rect.x;
			rect.x = 0;
		}
		if (rect.y < 0) {
			if (rect.y*-1 >= int(rect.h)) {
				continue;
			}
			rect.h += rect.y;
			rect.y = 0;
		}
		if (rect.x >= fb_width || rect.y >= fb_height) {
			continue;
		}
		if (rect.x + rect.w > fb_width) {
			rect.w = fb_width - rect.x;
		}
		if (rect.y + rect.h > fb_height) {
			rect.h = fb_height - rect.y;
		}

		update_rects.push_back(rect);
	}

	calc_rects();
*/
	int minx, maxx, miny, maxy;
	minx = fb_width;
	miny = fb_height;
	maxx = maxy = 0;
	for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& rect = *it;
		if (rect.x < minx) {
			minx = rect.x;
		}
		if (rect.x + rect.w > maxx) {
			maxx = rect.x + rect.w;
		}
		if (rect.y < miny) {
			miny = rect.y;
		}
		if (rect.y + rect.h > maxy) {
			maxy = rect.y + rect.h;
		}
	}
	if (minx < 0) {
		minx = 0;
	}
	if (miny < 0) {
		miny = 0;
	}
	SDL_Rect update_rect2 = ::create_rect(minx, miny, maxx - minx, maxy - miny);
	if (update_rect2.x + update_rect2.w > fb_width) {
		update_rect2.w = fb_width - update_rect2.x;
	}
	if (update_rect2.y + update_rect2.h > fb_height) {
		update_rect2.h = fb_height - update_rect2.y;
	}
	bufs_.first = update_rect2;
	
	return false;
}

void float_animation::set_scale(int w, int h, bool constrained, bool up)
{
	VALIDATE((w > 0 && h > 0) || !w && !h, "both w and h must large than 0 or equal 0!");
	std_w_ = w;
	std_h_ = h;
	constrained_scale_ = constrained;
	up_scale_ = up;
}

static bool compare_surf(const surface& surf1, const surface& surf2)
{
	if (surf1->w != surf2->w || surf1->h != surf2->h) {
		return false;
	}
	if (surf1->format != surf2->format) {
		return false;
	}

    static int times = 0;
    int line = 0, col = 0;
	{
		const_surface_lock lock1(surf1);
		const_surface_lock lock2(surf2);
		const Uint32* beg1 = lock1.pixels();
		const Uint32* end1 = beg1 + surf1->w * surf1->h;

		const Uint32* beg2 = lock2.pixels();

		while (beg1 != end1) {
			Uint32 pixel1 = *beg1;
			Uint32 pixel2 = *beg2;

			if (pixel1 != pixel2) {
				return false;
			}

			++ beg1;
			++ beg2;
            
            col ++;
            if (col == surf1->w) {
                line ++;
                col = 0;
            }
		}
	}

    times ++;
	return true;
}

static bool compare_score_circle_png(const texture& tex)
{
	surface surf1 = image::get_image("misc/score-circle.png");
	surface surf2 = save_texture_to_surface(tex);
	return compare_surf(surf1, surf2);
}

void float_animation::redraw(texture& screen, const SDL_Rect& rect, bool snap_bg)
{
	double scale_x = std_w_? 1.0 * rect.w / std_w_: 1.0;
	double scale_y = std_h_? 1.0 * rect.h / std_h_: 1.0;
	if (constrained_scale_ && scale_x != scale_y) {
		if (scale_x > scale_y) {
			scale_x = scale_y;
		} else {
			scale_y = scale_x;
		}
	}
	if (!up_scale_) {
		if (scale_x > 1) {
			scale_x = 1;
		}
		if (scale_y > 1) {
			scale_y = 1;
		}
	}
	anim2::rt.set(type_, scale_x, scale_y, rect);

	SDL_Renderer* renderer = get_renderer();
	const SDL_PixelFormat& format = get_neutral_pixel_format();
	int screen_width, screen_height;
	SDL_QueryTexture(screen.get(), NULL, NULL, &screen_width, &screen_height);

	animation::invalidate(screen_width, screen_height);
	const SDL_Rect& view_rect = bufs_.first;
	if (buf_use_texture) {
		VALIDATE(snap_bg || buf_tex_.get(), null_str);
		if (snap_bg) {
			buf_tex_ = SDL_CreateTexture(renderer, format.format, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);
		
			// below will change target. require save/recover clip setting of preview target.
			texture_clip_rect_setter clip(NULL);
/*
			{
				surface surf = image::get_image("misc/score-circle.png");

				texture screen2 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, surf->w, surf->h);
				SDL_SetTextureBlendMode(screen2.get(), SDL_BLENDMODE_BLEND);
				{
					trender_target_lock lock2(renderer, screen2);
					SDL_RenderClear(renderer);
        
					texture tex = SDL_CreateTextureFromSurface(renderer, surf);
					SDL_SetTextureBlendMode(tex.get(), SDL_BLENDMODE_NONE);
					SDL_Rect src_clip = ::create_rect(0, 0, surf->w, surf->h);
					SDL_Rect dst_clip = ::create_rect(0, 0, surf->w, surf->h);
					SDL_RenderCopyEx(renderer, tex.get(), &src_clip, &dst_clip, 0, NULL, SDL_FLIP_NONE);
				}
				compare_score_circle_png(screen2);
				compare_score_circle_png(screen);

				// ttexture_blend_none_lock lock(screen2);
				// SDL_RenderCopy(renderer, screen2.get(), NULL, NULL);
			}
*/
			// to copy alpha, must set src-alpha to blendmode_none.
			ttexture_blend_none_lock lock(screen);
			trender_target_lock lock2(renderer, buf_tex_);
        
			SDL_RenderCopy(renderer, screen.get(), NULL, NULL);
		}

	} else {
		VALIDATE(snap_bg || buf_surf_.get(), null_str);
		if (snap_bg) {
			buf_surf_ = create_neutral_surface(screen_width, screen_height);
			SDL_RenderReadPixels(renderer, NULL, buf_surf_->format->format, buf_surf_->pixels, 4 * screen_width);
		}
	}

	animation::redraw(screen, rect, false);
}

void float_animation::undraw(texture& screen)
{
	SDL_Renderer* renderer = get_renderer();

	if (buf_use_texture) {
		SDL_BlendMode mode;
		SDL_GetTextureBlendMode(buf_tex_.get(), &mode);
		VALIDATE(mode == SDL_BLENDMODE_NONE, null_str);

		SDL_RenderCopy(renderer, buf_tex_.get(), &bufs_.first, &bufs_.first);

	} else {
		SDL_UpdateTexture(screen.get(), &bufs_.first, 
			(const char*)buf_surf_->pixels + 4 * (bufs_.first.x + bufs_.first.y * buf_surf_->w), 4 * buf_surf_->w);
	}
}


float_animation* start_window_anim_th(int type, int* id)
{
	const animation* tpl = anim2::anim(type);
	if (!tpl) {
		return NULL;
	}

	float_animation* clone = new float_animation(*tpl);
	clone->set_type(anim_window);
	int _id = anim2::manager::instance->insert_area_anim2(clone);
	if (id) {
		*id = _id;
	}
	return clone;
}

void start_window_anim_bh(float_animation& anim, bool cycles)
{
	new_animation_frame();
	anim.start_animation(0, map_location::null_location, map_location::null_location, cycles);
}

int start_cycle_float_anim(const config& cfg)
{
	int type = anim2::find(cfg["id"]);
	if (type == nposm) {
		return nposm;
	}
	const animation* tpl = anim2::anim(type);
	if (!tpl) {
		return nposm;
	}
	float_animation* clone = new float_animation(*tpl);
		
	int id = anim2::manager::instance->insert_area_anim2(clone);

	float_animation& anim = *clone;
	anim.set_type(anim_canvas);
	anim.set_scale(cfg["width"].to_int(), cfg["height"].to_int(), cfg["constrained"].to_bool(), cfg["up"].to_bool(true));

	std::vector<std::string> vstr;
	BOOST_FOREACH (const config::attribute& attr, cfg.attribute_range()) {
		if (attr.first.find("-") == std::string::npos) {
			continue;
		}
		vstr = utils::split(attr.first, '-', utils::STRIP_SPACES);
		if (vstr.size() != 3) {
			continue;
		}
		anim_field_tag::tfield field = anim_field_tag::find(vstr[1]);
		if (field == anim_field_tag::NONE) {
			continue;
		}
		if (anim_field_tag::is_progressive_field(field)) {
			anim.replace_progressive(vstr[0], vstr[1], vstr[2], attr.second);

		} else if (field == anim_field_tag::IMAGE) {
			anim.replace_image_name(vstr[0], vstr[2], attr.second);

		} else if (field == anim_field_tag::IMAGE_MOD) {
			anim.replace_image_mod(vstr[0], vstr[2], attr.second);

		} else if (field == anim_field_tag::TEXT) {
			anim.replace_static_text(vstr[0], vstr[2], attr.second);

		}
	}

	new_animation_frame();
	// To cycle animation, all particular must start next cycle at same time!
	// see http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=143&extra=page%3D1
	anim.start_animation(anim.get_begin_time(), map_location::null_location, map_location::null_location, false);
	return id;
}

void draw_canvas_anim(int id, texture& canvas, const SDL_Rect& rect, bool snap_bg)
{
	if (id == nposm) {
		return;
	}
	float_animation& anim = *dynamic_cast<float_animation*>(&anim2::manager::instance->area_anim(id));
	if (!anim.started()) {
		return;
	}

	new_animation_frame();
	if (anim.animation_finished_potential()) {
		anim.start_animation(anim.get_begin_time(), map_location::null_location, map_location::null_location, false);
	} else {
		anim.update_last_draw_time();
	}
	anim.redraw(canvas, rect, snap_bg);

	anim2::manager::instance->drawing_buffer_commit(canvas, anim2::rt.rect);
}
