/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_EFFECT_HPP_INCLUDED
#define GUI_WIDGETS_EFFECT_HPP_INCLUDED

#include "gui/widgets/control.hpp"
#include "gui/widgets/stack.hpp"
#include "area_anim.hpp"

namespace gui2 {

class ttrack;

namespace effect {
class trefresh
{
public:
	enum {norefresh, refreshing, refreshed};
	enum {track_normal, track_drag, track_springback};

	trefresh()
		: widget_(NULL)
		, anim_type_(nposm)
		, mascot_anim_id_(nposm)
		, mascot_anim_rect_(null_rect)
		, egg_gap_(2 * twidget::hdpi_scale)
		, min_egg_diameter_(6 * twidget::hdpi_scale)
		, shape_egg_diameter_(24 * twidget::hdpi_scale)
		, max_egg_diameter_(66 * twidget::hdpi_scale)
		, image_("misc/egg.png")
		, track_status_(track_normal)
		, refresh_(norefresh)
		, yoffset_at_start_(0)
		, springback_yoffset_(0)
		, springback_granularity_(nposm)
		, refreshed_linger_ticks_(0)
		, first_coordinate_(nposm, nposm)
		, last_coordinate_(nposm, nposm)
	{}

	~trefresh() 
	{
		erase_mascot_anim();
	}

	void set_widget(ttrack* widget)
	{
		widget_ = widget;
	}

	int draw(const int yoffset);
	void set_refreshed() { refresh_ = refreshed; }

	int refresh_status() const { return refresh_; }
	// int track_status() const { return track_status_; }

	void signal_handler_left_button_down(tcontrol* control, const tpoint& coordinate);
	bool did_control_drag_detect(tcontrol* control, const tpoint& coordiante);

	void set_refreshing_anim(int anim) { anim_type_ = anim; }
	void set_refreshed_text(const std::string& text) { refreshed_text_ = text; }

	void set_did_refreshing(const boost::function<void ()>& fn) { did_refreshing_ = fn; }
	void set_did_can_drag(const boost::function<bool ()>& fn) { did_can_drag_ = fn; }

private:
	void start_mascot_anim(int x, int y);
	void erase_mascot_anim();

private:
	ttrack* widget_;
	int anim_type_;
	std::string image_;
	int mascot_anim_id_;
	SDL_Rect mascot_anim_rect_;
	std::string refreshed_text_;

	int egg_gap_;
	int min_egg_diameter_;
	int shape_egg_diameter_;
	int max_egg_diameter_; // must equal mascot_anim_rect_.h

	boost::function<void ()> did_refreshing_;
	boost::function<bool ()> did_can_drag_;

	tpoint first_coordinate_;
	tpoint last_coordinate_;
	int track_status_;
	int springback_yoffset_;
	int yoffset_at_start_;
	int refresh_;
	int springback_granularity_;

	uint32_t refreshed_linger_ticks_;
};

} // namespace effect
} // namespace gui2

#endif
