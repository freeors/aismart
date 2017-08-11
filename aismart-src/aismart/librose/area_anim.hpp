/* $Id: unit_display.hpp 47623 2010-11-21 13:57:27Z mordante $ */
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

/**
 *  @file
 *  Display units performing various actions: moving, attacking, and dying.
 */

#ifndef LIBROSE_AREA_ANIM_HPP_INCLUDED
#define LIBROSE_AREA_ANIM_HPP_INCLUDED

#include "config.hpp"
#include "animation.hpp"


namespace anim_field_tag {
enum tfield {NONE, X, Y, OFFSET_X, OFFSET_Y, IMAGE, IMAGE_MOD, TEXT, ALPHA};

bool is_progressive_field(tfield field);
tfield find(const std::string& tag);
const std::string& rfind(tfield tag);
}

namespace anim2 {

class manager: public tdrawing_buffer
{
public:
	static manager* instance;

	explicit manager(CVideo& video);
	~manager();

	void draw_window_anim();
	void undraw_window_anim();

private:
	double get_zoom_factor() const override;
	SDL_Rect clip_rect_commit() const override;
};

enum ttype {LOCATION, HSCROLL_TEXT, TITLE_SCREEN, 
	OPERATING, MIN_APP_ANIM};

int find(const std::string& tag);
const std::string& rfind(int at);

void fill_anims(const config& cfg);
const animation* anim(int at);
void utype_anim_create_cfg(const std::string& anim_renamed_key, const std::string& tpl_id, config& dst, const utils::string_map& symbols);

struct rt_instance
{
	void set(tanim_type _type, double _zoomx, double _zoomy, const SDL_Rect& _rect)
	{
		type = _type;
		zoomx = _zoomx;
		zoomy = _zoomy;
		rect = _rect;
	}

	tanim_type type;
	double zoomx;
	double zoomy;
	SDL_Rect rect;
};
extern rt_instance rt;

}

class float_animation: public animation
{
public:
	explicit float_animation(const animation& anim);

	void set_scale(int w, int h, bool constrained, bool up);

	bool invalidate(const int fb_width, const int fb_height, frame_parameters& value);
	void redraw(texture& screen, const SDL_Rect& rect, bool snap_bg);
	void undraw(texture& screen);

	void set_special_rect(const SDL_Rect& rect) { special_rect_ = rect; }
	const SDL_Rect& special_rect() const { return special_rect_; }

private:
	int std_w_;
	int std_h_;
	bool constrained_scale_;
	bool up_scale_;
	SDL_Rect special_rect_;
	std::pair<SDL_Rect, texture> bufs_;

	texture buf_tex_;
	surface buf_surf_;
};

float_animation* start_window_anim_th(int type, int* id);
void start_window_anim_bh(float_animation& anim, bool cycles);

int start_cycle_float_anim(const config& cfg);
void draw_canvas_anim(int id, texture& canvas, const SDL_Rect& rect, bool snap_bg);

#endif
