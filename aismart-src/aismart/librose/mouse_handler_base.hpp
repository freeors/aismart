/* $Id: mouse_handler_base.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef MOUSE_HANDLER_BASE_H_INCLUDED
#define MOUSE_HANDLER_BASE_H_INCLUDED

#include "map_location.hpp"
#include "SDL.h"

class display;

namespace events {

struct command_disabler
{
	command_disabler();
	~command_disabler();
};

extern int commands_disabled;

class mouse_handler_base 
{
public:
	mouse_handler_base();
	virtual ~mouse_handler_base() {}

	/**
	 * Reference to the used display objects. Derived classes should ensure
	 * this is always valid. Note the constructor of this class cannot use this.
	 */
	virtual display& gui() = 0;

	/**
	 * Const version.
	 */
	virtual const display& gui() const = 0;

	bool is_dragging_left() const { return dragging_left_; }

	/**
	 * @return true when the class in the "dragging" state.
	 */
	bool is_dragging() const;

	//minimum dragging distance to fire the drag&drop
	virtual int drag_threshold() const {return 0;};

	bool get_show_menu() const { return show_menu_; }

	bool is_moving() const { return moving_; }
	bool is_selecting() const { return selecting_; }

	void current_position(int* x, int* y);

	/**
	 * This handles minimap scrolling and click-drag.
	 * @returns true when the caller should not process the mouse motion
	 * further (i.e. should return), false otherwise.
	 */
	bool mouse_motion_default(int x, int y);

	void mouse_press(const SDL_MouseButtonEvent& event);
	bool is_left_click(const SDL_MouseButtonEvent& event) const;
	bool is_middle_click(const SDL_MouseButtonEvent& event) const;
	bool is_right_click(const SDL_MouseButtonEvent& event) const;

	/**
	 * Derived classes can overrid this to disable mousewheel scrolling under
	 * some circumstances, e.g. when the mouse wheel controls something else,
	 * but the event is also received by this class
	 */
	virtual bool allow_mouse_wheel_scroll(int x, int y);

	/**
	 * Called whenever the left mouse drag has "ended".
	 */
	virtual void left_drag_end(int x, int y) {}

	/**
	 * Called whenever the right mouse drag has "ended".
	 */
	virtual void right_drag_end(int x, int y) {}

	void cancel_dragging();
	bool minimap_left_middle_down(const int x, const int y);

protected:
	void clear_dragging(const SDL_MouseButtonEvent& event);
	void init_dragging(bool& dragging_flag);

protected:
	/** minimap scrolling (scroll-drag) state flag */
	bool minimap_scrolling_;
	/** LMB drag init flag */
	bool dragging_left_;
	/** Actual drag flag */
	bool dragging_started_;
	/** RMB drag init flag */
	bool dragging_right_;
	/** Drag start position x */
	int drag_from_x_;
	/** Drag start position y */
	int drag_from_y_;
	/** Drag start map location */
	map_location drag_from_hex_;

	/** last highlighted hex */
	map_location last_hex_;

	/** mouse/finger motion to/hit at position x */
	int x_;
	/** mouse/finger motion to /hit at position y */
	int y_;

	/** Show context menu flag */
	bool show_menu_;

	bool moving_;
	bool expediting_;
	bool selecting_;
};

} // end namespace events

#endif
