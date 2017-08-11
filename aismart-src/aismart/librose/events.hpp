/* $Id: events.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef EVENTS_HPP_INCLUDED
#define EVENTS_HPP_INCLUDED

#include "SDL.h"
#include "util.hpp"
#include <memory> // std::unique_ptr

// our user-defined timer event type
#define SDL_LONGPRESS	SDL_USEREVENT
#define TIMER_EVENT		(SDL_USEREVENT + 1)

enum HOTKEY_COMMAND {
	HOTKEY_NULL,
	HOTKEY_ZOOM_IN, HOTKEY_ZOOM_OUT, HOTKEY_ZOOM_DEFAULT,
	HOTKEY_SCREENSHOT, HOTKEY_MAP_SCREENSHOT,
	HOTKEY_COPY, HOTKEY_PASTE, HOTKEY_CUT,
	HOTKEY_CHAT, HOTKEY_UNDO, HOTKEY_REDO, HOTKEY_HELP, HOTKEY_SYSTEM,
	HOTKEY_MIN = 100
};

struct tfinger
{
	tfinger(SDL_FingerID id, int x, int y, Uint32 active)
		: fingerId(id)
		, x(x)
		, y(y)
		, active(active)
	{}


	bool operator==(const tfinger& that) const
	{
		if (fingerId != that.fingerId) return false;
		return true;
	}
	bool operator!=(const tfinger& that) const { return !operator==(that); }


	SDL_FingerID fingerId;
	int x;
	int y;
	Uint32 active;

	struct tmotion {
		tmotion(uint32_t ticks, int x, int y, int dx, int dy)
			: ticks(ticks)
			, x(x)
			, y(y)
			, dx(dx)
			, dy(dy)
		{}

		uint32_t ticks;
		int x;
		int y;
		int dx;
		int dy;
	};
	std::vector<tmotion> motions;
};

// tevent_dispatcher has tow task.
// 1. join/leave context.
// 2. process mouse/finger events.
class tevent_dispatcher
{
public:
	static const int swipe_wheel_level_gap;
	static const int swipe_max_normal_level;
	static int delta_2_level(const int delta, const bool finger);

	void handle_event(const SDL_Event& event);

	// generate multigesture whether or not.
	// because of up nest, don't use variable.
	bool multi_gestures() const { return fingers_.size() >= 2; }

	virtual bool finger_coordinate_valid(int x, int y) const { return true; }
	virtual bool mouse_wheel_coordinate_valid(int x, int y) const { return true; }

	virtual void handle_pinch(const int x, const int y, const bool out) {}
	virtual void handle_swipe(const int x, const int y, const int xlevel, const int ylevel) {}
	virtual void handle_mouse_down(const SDL_MouseButtonEvent& button) {}
	virtual void handle_mouse_up(const SDL_MouseButtonEvent& button) {}
	virtual void handle_mouse_motion(const SDL_MouseMotionEvent& motion) {}
	virtual void handle_longpress(const int x, const int y) {};

	void mouse_leave_window();

protected:
	void join(); /*joins the current event context*/
	void leave(); /*leave the event context*/

	tevent_dispatcher(const bool scene, const bool auto_join);
	virtual ~tevent_dispatcher();

private:
	// true: allow continue to process this event, else return false.
	virtual bool mini_pre_handle_event(const SDL_Event& event) = 0;
	virtual void mini_handle_event(const SDL_Event& event) = 0;
	void send_extra_mouse_motion(const int x, const int y);

protected:
	const bool scene_;
	std::vector<std::unique_ptr<tfinger> > fingers_;
	int pinch_distance_;
	int mouse_motions_;
	Uint32 pinch_noisc_time_;
	Uint32 last_pinch_ticks_;
	tpoint last_motion_coordinate_;

private:
	bool has_joined_;
};

namespace events
{

//causes events to be dispatched to all handler objects.
void pump();

class pump_monitor {
//pump_monitors receive notifcation after an events::pump() occurs
public:
	pump_monitor(bool auto_join = true);
	virtual ~pump_monitor();
	virtual void monitor_process() = 0;
	void join(); /*joins the current monitor*/
private:
	bool has_joined_;
};

int discard(Uint32 event_mask_min, Uint32 event_mask_max);
}

#define INPUT_MASK_MIN SDL_KEYDOWN
#define INPUT_MASK_MAX SDL_MULTIGESTURE

#endif
