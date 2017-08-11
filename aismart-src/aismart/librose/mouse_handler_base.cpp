/* $Id: mouse_handler_base.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "mouse_handler_base.hpp"

#include "cursor.hpp"
#include "display.hpp"
#include "preferences.hpp"

namespace events {

command_disabler::command_disabler()
{
	++commands_disabled;
}

command_disabler::~command_disabler()
{
	--commands_disabled;
}

int commands_disabled= 0;

static bool command_active()
{
	return false;
}

mouse_handler_base::mouse_handler_base() 
	: minimap_scrolling_(false)
	, dragging_left_(false)
	, dragging_started_(false)
	, dragging_right_(false)
	, drag_from_x_(0)
	, drag_from_y_(0)
	, x_(0)
	, y_(0)
	, drag_from_hex_()
	, last_hex_()
	, show_menu_(false)
	, moving_(false)
	, expediting_(false)
	, selecting_(false)
{
}

bool mouse_handler_base::is_dragging() const
{
	return dragging_left_ || dragging_right_;
}

bool mouse_handler_base::mouse_motion_default(int x, int y)
{
	x_ = x;
	y_ = y;
	if (minimap_scrolling_) {
		//if the game is run in a window, we could miss a LMB/MMB up event
		// if it occurs outside our window.
		// thus, we need to check if the LMBis still down
		minimap_scrolling_ = (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
		if (minimap_scrolling_) {
			const map_location& loc = gui().minimap_location_on(x,y);
			if (loc.valid()) {
				if(loc != last_hex_) {
					last_hex_ = loc;
					gui().scroll_to_tile(loc,display::WARP,false);
				}
			} else {
				// clicking outside of the minimap will end minimap scrolling
				minimap_scrolling_ = false;
			}
		}
		if (minimap_scrolling_) {
			return true;
		}
	}

	// Fire the drag & drop only after minimal drag distance
	// While we check the mouse buttons state, we also grab fresh position data.
	int mx = drag_from_x_; // some default value to prevent unlikely SDL bug
	int my = drag_from_y_;
	if (is_dragging() && !dragging_started_) {
		if ((dragging_left_ && (SDL_GetMouseState(&mx,&my) & SDL_BUTTON_LEFT) != 0)
		|| (dragging_right_ && (SDL_GetMouseState(&mx,&my) & SDL_BUTTON_RIGHT) != 0)) {
			const double drag_distance = std::pow(static_cast<double>(drag_from_x_- mx), 2)
					+ std::pow(static_cast<double>(drag_from_y_- my), 2);
			if (drag_distance > drag_threshold()*drag_threshold()) {
				dragging_started_ = true;
				cursor::set_dragging(true);
			}
		}
	}
	return false;
}

void mouse_handler_base::current_position(int* x, int* y)
{
	if (x) {
		*x = x_;
	}
	if (y) {
		*y = y_;
	}
}

void mouse_handler_base::mouse_press(const SDL_MouseButtonEvent& event)
{
	x_ = event.x;
	y_ = event.y;

	show_menu_ = false;

	if (is_left_click(event)) {
		if (event.state == SDL_PRESSED) {
			cancel_dragging();
			init_dragging(dragging_left_);

		} else if (event.state == SDL_RELEASED) {
			minimap_scrolling_ = false;
			clear_dragging(event);
		}
	} else if (is_right_click(event)) {
		if (event.state == SDL_PRESSED) {
			cancel_dragging();
			init_dragging(dragging_right_);

		} else if (event.state == SDL_RELEASED) {
			minimap_scrolling_ = false;
			clear_dragging(event);
		}
	} else if (is_middle_click(event)) {
		// wesnoth implete middle down/motion same as left, I cancel it.
		if (event.state == SDL_PRESSED) {

		} else if (event.state == SDL_RELEASED) {

		}
	}

	if (!dragging_left_ && !dragging_right_ && dragging_started_) {
		dragging_started_ = false;
		cursor::set_dragging(false);
	}
}

bool mouse_handler_base::is_left_click(
		const SDL_MouseButtonEvent& event) const
{
	return event.button == SDL_BUTTON_LEFT && !command_active();
}

bool mouse_handler_base::is_middle_click(
		const SDL_MouseButtonEvent& event) const
{
	return event.button == SDL_BUTTON_MIDDLE;
}

bool mouse_handler_base::is_right_click(
		const SDL_MouseButtonEvent& event) const
{
	return event.button == SDL_BUTTON_RIGHT
			|| (event.button == SDL_BUTTON_LEFT && command_active());
}

bool mouse_handler_base::allow_mouse_wheel_scroll(int /*x*/, int /*y*/)
{
	return true;
}

bool mouse_handler_base::minimap_left_middle_down(const int x, const int y)
{
	display& disp = gui();
	const map_location& loc = disp.minimap_location_on(x, y);
	minimap_scrolling_ = false;
	if (loc.valid()) {
		minimap_scrolling_ = true;
		last_hex_ = loc;
		disp.scroll_to_tile(loc, display::WARP, false);
		return true;
	}
	return false;
}

void mouse_handler_base::init_dragging(bool& dragging_flag)
{
	dragging_flag = true;
	SDL_GetMouseState(&drag_from_x_, &drag_from_y_);
	drag_from_hex_ = gui().screen_2_loc(drag_from_x_, drag_from_y_);
}

void mouse_handler_base::cancel_dragging()
{
	dragging_started_ = false;
	dragging_left_ = false;
	dragging_right_ = false;
	cursor::set_dragging(false);
}

void mouse_handler_base::clear_dragging(const SDL_MouseButtonEvent& event)
{
	// we reset dragging info before calling functions
	// because they may take time to return, and we
	// could have started other drag&drop before that
	cursor::set_dragging(false);
	if (dragging_started_) {
		dragging_started_ = false;
		if (dragging_left_) {
			dragging_left_ = false;
			left_drag_end(event.x, event.y);
		}
		if (dragging_right_) {
			dragging_right_ = false;
			right_drag_end(event.x, event.y);
		}
	} else {
		dragging_left_ = false;
		dragging_right_ = false;
	}
}


} //end namespace events
