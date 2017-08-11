/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/effect.hpp"
#include "gui/widgets/track.hpp"

#include "area_anim.hpp"
#include "font.hpp"
#include "gettext.hpp"

#include <boost/bind.hpp>
#include <algorithm>

namespace gui2 {

namespace effect {

void trefresh::start_mascot_anim(int x, int y)
{
	if (anim_type_ == nposm) {
		return;
	}
	if (is_null_rect(mascot_anim_rect_)) {
		surface surf = image::get_image("misc/mascot.png");
		mascot_anim_rect_ = ::create_rect(x, y, surf->w, surf->h);
	}

	if (mascot_anim_id_ == nposm) {
		float_animation* anim = start_window_anim_th(anim_type_, &mascot_anim_id_);
		anim->set_scale(0, 0, true, false);
		anim->set_special_rect(mascot_anim_rect_);

		start_window_anim_bh(*anim, true);

	} else {
		float_animation* anim = dynamic_cast<float_animation*>(&anim2::manager::instance->area_anim(mascot_anim_id_));
		mascot_anim_rect_.x = x;
		mascot_anim_rect_.y = y;
		anim->set_special_rect(mascot_anim_rect_);
	}
}

void trefresh::erase_mascot_anim()
{
	if (mascot_anim_id_ != nposm) {
		anim2::manager::instance->erase_area_anim(mascot_anim_id_);
		mascot_anim_id_ = nposm;
	}
}

void trefresh::signal_handler_left_button_down(tcontrol* control, const tpoint& coordinate)
{
	if (did_can_drag_ && !did_can_drag_()) {
		return;
	}
	springback_granularity_ = nposm;
	track_status_ = track_drag;
	// maybe start drag when springback.
	yoffset_at_start_ = springback_yoffset_;

	first_coordinate_ = coordinate;
}

bool trefresh::did_control_drag_detect(tcontrol* control, const tpoint& last)
{
	if (track_status_ == track_drag) {
		springback_yoffset_ = last.y - last.y + yoffset_at_start_;
		track_status_ = track_springback;
	}
	last_coordinate_ = last;
	return false;
}

int trefresh::draw(const int _set_yoffset)
{
	int set_yoffset = _set_yoffset;
	set_yoffset += yoffset_at_start_;
	if (_set_yoffset == 0) {
		if (track_status_ == track_drag) {
			set_yoffset = last_coordinate_.y - first_coordinate_.y;
			set_yoffset += yoffset_at_start_;

		} else if (track_status_ == track_springback) {
			// recalculate springback_yoffset_
			if (springback_yoffset_ > 0) {
				if (springback_granularity_ == nposm) {
					springback_granularity_ = springback_yoffset_ / 6;
				}
				springback_yoffset_ -= springback_granularity_;
			}
			if (refresh_ == refreshing) {
				if (springback_yoffset_ < max_egg_diameter_ + egg_gap_) {
					springback_yoffset_ = max_egg_diameter_ + egg_gap_;
				}
			} else {
				if (refresh_ == refreshed) {
					// linger some time if refreshed.
					uint32_t now = SDL_GetTicks();
					if (refreshed_linger_ticks_ == 0 && springback_yoffset_ <= max_egg_diameter_ + egg_gap_) {
						refreshed_linger_ticks_ = now + 500; // 500 msecond
						springback_granularity_ = max_egg_diameter_ / 4;
					}
					if (now < refreshed_linger_ticks_) {
						springback_yoffset_ = max_egg_diameter_ + egg_gap_;
					}
				}
				if (springback_yoffset_ <= 0) {
					track_status_ = track_normal;
					refresh_ = norefresh;
					springback_yoffset_ = 0;
					yoffset_at_start_ = 0;
					refreshed_linger_ticks_ = 0;
				}
			}
			set_yoffset = springback_yoffset_;
		}
	}

	const SDL_Rect widget_rect = widget_->get_rect();
	const int xsrc = widget_rect.x;
	const int ysrc = widget_rect.y;
	SDL_Rect dst;

	SDL_Renderer* renderer = get_renderer();
	if (refresh_ == norefresh) {
		if (set_yoffset > egg_gap_) {
			surface surf = image::get_image(image_);
			if (surf) {
				int width = set_yoffset - egg_gap_, height = set_yoffset - egg_gap_;
				if (set_yoffset >= shape_egg_diameter_ + egg_gap_) {
					width = shape_egg_diameter_;
				}
				dst = ::create_rect(xsrc + (widget_rect.w - width) / 2, ysrc + egg_gap_, width, height);

				render_surface(renderer, surf, NULL, &dst);
			}
		}
		if (set_yoffset >= max_egg_diameter_ + egg_gap_) {
			refresh_ = refreshing;
			if (did_refreshing_) {
				did_refreshing_();
			}
		}

		if (mascot_anim_id_ != nposm && set_yoffset == 0) {
			erase_mascot_anim();
		}

	} else if (refresh_ == refreshing) {
		// VALIDATE(set_yoffset >= egg_gap_ + max_egg_diameter_, null_str);
		start_mascot_anim(xsrc + (widget_rect.w - mascot_anim_rect_.w) / 2, ysrc + set_yoffset - max_egg_diameter_);

	} else {
		if (mascot_anim_id_ != nposm) {
			erase_mascot_anim();
		}

		// refreshed
		if (refreshed_text_.empty()) {
			refreshed_text_ = _("Refresh successfully");
		}
		surface text_surf = font::get_rendered_text(refreshed_text_, 0, 32, font::NORMAL_COLOR);
		dst = ::create_rect(xsrc + (widget_rect.w - text_surf->w) / 2, ysrc + set_yoffset - ((max_egg_diameter_ - text_surf->h) / 2 + text_surf->h), text_surf->w, text_surf->h);
		render_surface(renderer, text_surf, NULL, &dst);
	}

	return set_yoffset;
}

} // namespace effect

} // namespace gui2

