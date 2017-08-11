/* $Id: events.cpp 46186 2010-09-01 21:12:38Z silene $ */
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

#include "global.hpp"

#include "cursor.hpp"
#include "events.hpp"
#include "sound.hpp"
#include "video.hpp"
#include "game_end_exceptions.hpp"
#include "display.hpp"
#include "preferences.hpp"
#include "gui/widgets/settings.hpp"
#include "base_instance.hpp"

#include "SDL.h"

#include <algorithm>
#include <utility>
#include <iomanip>

struct tevents_context
{
	tevents_context() :
		handlers(),
		focused_handler(-1)
	{
	}

	void add_handler(tevent_dispatcher* ptr);
	void remove_handler(tevent_dispatcher* ptr);

	// pump require enum all handler, so can not use std::stack
	std::vector<tevent_dispatcher*> handlers;
	int focused_handler;
};

void tevents_context::add_handler(tevent_dispatcher* ptr)
{
	handlers.push_back(ptr);
}

void tevents_context::remove_handler(tevent_dispatcher* ptr)
{
	// now only exsit tow handler.
	// when tow handler, must first is base_controller, second is gui2::event::thandler. leave must inverted sequence.
	VALIDATE(handlers.back() == ptr, null_str);

	handlers.pop_back();
}
 
static tevents_context contexts;

const int tevent_dispatcher::swipe_wheel_level_gap = 100;
const int tevent_dispatcher::swipe_max_normal_level = 10; // must be 10

// @delta: devided by hdpi_scale.
// return 0, no action.
int tevent_dispatcher::delta_2_level(const int delta, const bool finger)
{
	int ret;
	int abs_delta = abs(delta);
	if (finger) {
		if (abs_delta < 10) {
			return 0; // ===> 0
		} else if (abs_delta < 70) {
			ret =  1 + (abs_delta - 10) / 6; // ===> [1, 10]
		} else {
			ret = swipe_max_normal_level + 1; // ===> >10
		}
	} else {
		if (!delta) {
			return 0;
		}
		// on window, noral delta is 1. expand it to 1/3 content_->h
		ret = abs_delta * 3 + swipe_wheel_level_gap;
	}
	return delta > 0? ret: (-1 * ret);
}

tevent_dispatcher::tevent_dispatcher(const bool scene, const bool auto_join) 
	: scene_(scene)
	, has_joined_(false)
	, pinch_distance_(0)
	, mouse_motions_(0)
	, pinch_noisc_time_(100)
	, last_pinch_ticks_(0)
	, last_motion_coordinate_(nposm, nposm)
{
	if (auto_join) {
		contexts.add_handler(this);
		has_joined_ = true;
	}
}

tevent_dispatcher::~tevent_dispatcher()
{
	if (has_joined_) {
		leave();
	}
}

void tevent_dispatcher::join()
{
	// must not join more!
	VALIDATE(!has_joined_, null_str);

	// join self
	contexts.add_handler(this);
	has_joined_ = true;
}

void tevent_dispatcher::leave()
{
	VALIDATE(has_joined_ && !contexts.handlers.empty(), null_str);

	contexts.remove_handler(this);
	has_joined_ = false;
}

void tevent_dispatcher::handle_event(const SDL_Event& event)
{
	if (!mini_pre_handle_event(event)) {
		return;
	}

	if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERMOTION && event.type != SDL_FINGERUP
		&& event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP && event.type != SDL_MOUSEMOTION
		&& event.type != SDL_MULTIGESTURE && event.type != SDL_MOUSEWHEEL
		&& event.type != SDL_LONGPRESS) {
		mini_handle_event(event);
		return;
	}

	const int max_motions = 3;
	int x, y, dx, dy;
	bool hit = false;
	Uint8 mouse_flags;
	Uint32 now = SDL_GetTicks();

	unsigned screen_width2 = gui2::settings::screen_width;
	unsigned screen_height2 = gui2::settings::screen_height;

#if (defined(__APPLE__) && TARGET_OS_IPHONE)
	screen_width2 /= gui2::twidget::hdpi_scale;
	screen_height2 /= gui2::twidget::hdpi_scale;
#endif

	switch(event.type) {
	case SDL_FINGERDOWN:
		x = event.tfinger.x * screen_width2;
		y = event.tfinger.y * screen_height2;

		// SDL_Log("%i, SDL_FINGERDOWN, (%i, %i)\n", now, x, y);

		if (!finger_coordinate_valid(x, y)) {
			return;
		}
		fingers_.push_back(std::unique_ptr<tfinger>(new tfinger(event.tfinger.fingerId, x, y, now)));

		break;

	case SDL_FINGERMOTION:
		{
			int x1 = 0, y1 = 0, x2 = 0, y2 = 0, at = 0;
			x = event.tfinger.x * screen_width2;
			y = event.tfinger.y * screen_height2;
			dx = event.tfinger.dx * screen_width2;
			dy = event.tfinger.dy * screen_height2;

			// SDL_Log("%i, SDL_FINGERMOTION, (%i, %i), delta(%i, %i)\n", now, x, y, dx, dy);

			for (std::vector<std::unique_ptr<tfinger> >::iterator it = fingers_.begin(); it != fingers_.end(); ++ it, at ++) {
				tfinger& finger = **it;
				if (finger.fingerId == event.tfinger.fingerId) {
					finger.x = x;
					finger.y = y;
					finger.active = now;
					if (finger.motions.size() >= max_motions) {
						finger.motions.erase(finger.motions.begin());
					}
					finger.motions.push_back(tfinger::tmotion(now, x, y, dx, dy));
					
					hit = true;
				}
				if (at == 0) {
					x1 = finger.x;
					y1 = finger.y;
				} else if (at == 1) {
					x2 = finger.x;
					y2 = finger.y;
				}
			}
			if (!hit) {
				return;
			}
			if (!finger_coordinate_valid(x, y)) {
				return;
			}
			
			if (fingers_.size() == 2) {
				// calculate distance between finger
				int distance = (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
				if (pinch_distance_ != nposm) {
					int diff = pinch_distance_ - distance;
					if (diff) {
						int abs_diff = sqrt(abs(diff));
						const int pinch_threshold = 80;
						if (abs_diff >= pinch_threshold * gui2::twidget::hdpi_scale) {
							pinch_distance_ = distance;
							if (now - last_pinch_ticks_ > pinch_noisc_time_) {
								last_pinch_ticks_ = now;
								handle_pinch(x, y, diff > 0);
							}
						}
					}
				} else {
					pinch_distance_ = distance;
				}
			}
		}
		break;

	case SDL_FINGERUP:
		// x = event.tfinger.x * screen_width2;
		// y = event.tfinger.y * screen_height2;
		// SDL_Log("%i, SDL_FINGERUP, (%i, %i)\n", now, x, y);
		// SDL_MOUSEBUTTONUP has do anything relative of up, here do nothing.
		break;

	case SDL_MULTIGESTURE:
		// Now I don't use SDL logic, process multi-finger myself. Ignore it.
		break;

	case SDL_MOUSEBUTTONDOWN:
		mouse_motions_ = 0;
		pinch_distance_ = nposm;

		// SDL_Log("(%s)SDL_MOUSEBUTTONDOWN[#%i], (%i, %i), last_motion(%i, %i)\n", scene_? "scene": "gui2", event.button.button, event.button.x, event.button.y, last_motion_coordinate_.x, last_motion_coordinate_.y);

		send_extra_mouse_motion(event.button.x, event.button.y);
		handle_mouse_down(event.button);
		break;

	case SDL_MOUSEBUTTONUP:
		if (fingers_.size() == 1) {
			const uint32_t threshold_span = 120; // 120ms
			int x2 = 0, y2 = 0, dx = 0, dy = 0;
			const tfinger& finger = *fingers_[0].get();
			for (std::vector<tfinger::tmotion>::const_iterator it = finger.motions.begin(); it != finger.motions.end(); ++ it) {
				const tfinger::tmotion& motion = *it;
				if (now - motion.ticks <= threshold_span) {
					dx += motion.dx;
					dy += motion.dy;
					x2 += motion.x;
					y2 += motion.y;
				}
			}
			if (!finger.motions.empty()) {
				x2 /= finger.motions.size();
				y2 /= finger.motions.size();
			}

			int xlevel = delta_2_level(dx / gui2::twidget::hdpi_scale, true);
			int ylevel = delta_2_level(dy / gui2::twidget::hdpi_scale, true);
			if ((xlevel || ylevel) && finger_coordinate_valid(x2, y2)) {
				handle_swipe(x2, y2, xlevel, ylevel);
			}
		}

		// SDL_Log("(%s)SDL_MOUSEBUTTONUP[#%i], (%i, %i)\n", scene_? "scene": "gui2", event.button.button, event.button.x, event.button.y);
		{
			// 1. once first finger up, think those finger end.
			// 2. solve mouse_up nest.
			const bool multi_gestures2 = multi_gestures();
			fingers_.clear();

			if (!multi_gestures2) {
				send_extra_mouse_motion(event.button.x, event.button.y);
				handle_mouse_up(event.button);
			} else {
				// SDL_Log("(%s)SDL_MOUSEBUTTONUP, (%i, %i), multi_gestures2 is true, send multi_gestures_up\n", scene_? "scene": "gui2");
				// in order to keep down/up couple
				SDL_MouseButtonEvent button;
				button.type = event.button.type;
				button.button = event.button.button;
				set_multi_gestures_up(button);
				handle_mouse_up(button);
			}
		}
		if (event.button.button == SDL_BUTTON_LEFT) {
			last_motion_coordinate_ = tpoint(nposm, nposm);
		}

		break;

	case SDL_MOUSEMOTION:
		// git rid of exception motion
		dx = abs(event.motion.xrel);
		dy = abs(event.motion.yrel);
		if (dx >= (int)gui2::settings::screen_width || dy >= (int)gui2::settings::screen_height) {
			break;
		}
		if (dx || dy) {
			mouse_motions_ ++;
		}

		// SDL_Log("SDL_MOUSEMOTION, xy(%i, %i), ref(%i, %i)\n", event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
		if (!multi_gestures()) {
			last_motion_coordinate_ = tpoint(event.motion.x, event.motion.y);
			handle_mouse_motion(event.motion);
		}
		break;

	case SDL_MOUSEWHEEL:
		// I think mobile device cannot send it.
		VALIDATE(!game_config::mobile, null_str);

		// SDL_Log("SDL_MOUSEWHEEL, wheel(%i, %i), which(%i), direction: %i\n", event.wheel.x, event.wheel.y, event.wheel.which, event.wheel.direction);

		mouse_flags = SDL_GetMouseState(&x, &y);
		if (!mouse_wheel_coordinate_valid(x, y)) {
			return;
		}
#ifdef _WIN32
		if (mouse_flags & SDL_BUTTON(SDL_BUTTON_RIGHT) && abs(event.wheel.y) >= MOUSE_MOTION_THRESHOLD) {
			// left mouse + wheel vetical ==> pinch
			mouse_motions_ ++;
			Uint32 now = SDL_GetTicks();
			if (now - last_pinch_ticks_ > pinch_noisc_time_) {
				last_pinch_ticks_ = now;
				handle_pinch(x, y, event.wheel.y > 0);
			}
			
		} else
#endif
		if (finger_coordinate_valid(x, y)) {
			// i think: offset = end - start. so, if end > start, offset should be positive. but wheel is by contraries.
			dx = event.wheel.x;
			dy = event.wheel.y;
			if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
				dx *= -1;
				dy *= -1;
			}
			int xlevel = delta_2_level(dx, false);
			int ylevel = delta_2_level(dy, false);
			if (xlevel || ylevel) {
				handle_swipe(x, y, xlevel, ylevel);
			}
		}
		break;

	case SDL_LONGPRESS:
		if (fingers_.size() > 1) {
			// PC hasn't fingers. but they have longpress.
			break;
		}
		handle_longpress(reinterpret_cast<long>(event.user.data1), reinterpret_cast<long>(event.user.data2));
		break;
	}
}

void tevent_dispatcher::mouse_leave_window()
{
	// SDL_Log("(%s)mouse_leave_window\n", scene_? "scene": "gui2");

	last_motion_coordinate_ = tpoint(nposm, nposm);
/*
	SDL_MouseMotionEvent motion;
	motion.x = motion.y = nposm;
	handle_mouse_motion(motion);
*/
}

void tevent_dispatcher::send_extra_mouse_motion(const int x, const int y)
{
	if (last_motion_coordinate_.x == x && last_motion_coordinate_.y == y) {
		return;
	}
	// SDL_Log("(%s)last_motion(%i, %i) isn't desire coordiante(%i, %i), require send extra mouse motion\n", scene_? "scene": "gui2", last_motion_coordinate_.x, last_motion_coordinate_.y, x, y);

	SDL_MouseMotionEvent motion;
	motion.x = x;
	motion.y = y;
	if (last_motion_coordinate_.x == nposm && last_motion_coordinate_.y == nposm) {
		motion.xrel = 0; 
		motion.yrel = 0;
	} else {
		motion.xrel = x - last_motion_coordinate_.x; 
		motion.yrel = y - last_motion_coordinate_.y;
	}
	handle_mouse_motion(motion);
}


namespace events
{

std::vector<pump_monitor*> pump_monitors;

pump_monitor::pump_monitor(bool auto_join)
	: has_joined_(false)
{
	if (auto_join) {
		join();
	}
}

pump_monitor::~pump_monitor()
{
	pump_monitors.erase(
		std::remove(pump_monitors.begin(), pump_monitors.end(), this),
		pump_monitors.end());
}

void pump_monitor::join()
{
	if (has_joined_) {
		return;
	}
	pump_monitors.push_back(this);
	has_joined_ = true;
}

static SDL_UserEvent longpress_event {SDL_LONGPRESS, 0, 0, 0, nullptr, nullptr};
static tpoint longpress_down(0, 0);

void insert_longpress_event(std::vector<SDL_Event>& events)
{
	if (!longpress_event.timestamp) {
		return;
	}
	if ((int)(SDL_GetTicks() - longpress_event.timestamp) < gui2::settings::longpress_time) {
		return;
	}

	SDL_Event event;
	event.type = SDL_LONGPRESS;
	event.user = longpress_event;

	events.push_back(event);
	longpress_event.timestamp = 0;
}

void dump_events(const std::vector<SDL_Event>& events)
{
	std::map<int, int> dump;
	for (std::vector<SDL_Event>::const_iterator it = events.begin(); it != events.end(); ++ it) {
		int type = it->type;

		std::map<int, int>::iterator find = dump.find(type);
		if (dump.find(type) != dump.end()) {
			find->second ++;
		} else {
			dump.insert(std::make_pair(type, 1));
		}
	}
	std::stringstream ss;
	for (std::map<int, int>::const_iterator it = dump.begin(); it != dump.end(); ++ it) {
		if (it != dump.begin()) {
			ss << "; ";
		}
		ss << "(type: " << std::setbase(16) << it->first << std::setbase(10) << ", count: " << it->second << ")";
	}
	SDL_Log("%s\n", ss.str().c_str());
}

void pump()
{
	if (instance->terminating()) {
		// let main thread throw quit exception.
		throw CVideo::quit();
	}

	SDL_Event temp_event;
	int poll_count = 0;
	int begin_ignoring = 0;

	std::vector<SDL_Event> events;
	std::vector<const SDL_Event*> timer_events;
	// ignore user input events when receive SDL_WINDOWEVENT. include before and after.
	while (SDL_PollEvent(&temp_event)) {
		if (temp_event.type == TIMER_EVENT && !timer_events.empty()) {
			// ignore repeated TIMER_EVENT, repeated is same data1/data2.
			bool found = false;
			for (std::vector<const SDL_Event*>::const_iterator it = timer_events.begin(); it != timer_events.end(); ++ it) {
				const SDL_Event& e = **it;
				if (e.user.data1 == temp_event.user.data1 && e.user.data2 == temp_event.user.data2) {
					found = true;
					break;
				}
			}
			if (found) {
				continue;
			}
		}

		++ poll_count;
		if (!begin_ignoring && temp_event.type == SDL_WINDOWEVENT) {
			begin_ignoring = poll_count;
		} else if (begin_ignoring > 0 && temp_event.type >= INPUT_MASK_MIN && temp_event.type <= INPUT_MASK_MAX) {
			//ignore user input events that occurred after the window was activated
			continue;
		}

		events.push_back(temp_event);
		if (temp_event.type == TIMER_EVENT) {
			timer_events.push_back(&events.back());
		}
	}

	if (events.size() > 10) {
		SDL_Log("------waring!! events.size(): %u, last_event: %x\n", events.size(), events.back().type);
		dump_events(events);
	}

	std::vector<SDL_Event>::iterator ev_it = events.begin();
	for (int i = 1; i < begin_ignoring; ++i) {
		if (ev_it->type >= INPUT_MASK_MIN && ev_it->type <= INPUT_MASK_MAX) {
			//ignore user input events that occurred before the window was activated
			ev_it = events.erase(ev_it);
		} else {
			++ev_it;
		}
	}
	if (events.empty()) {
		// events maybe exist event relative mouse. so only insert when empty.
		insert_longpress_event(events);
	}

	std::vector<SDL_Event>::iterator ev_end = events.end();
	for (ev_it = events.begin(); ev_it != ev_end; ++ev_it){
		SDL_Event& event = *ev_it;
		switch (event.type) {
			case SDL_APP_WILLENTERFOREGROUND:
			case SDL_APP_DIDENTERFOREGROUND:
				// first these event maybe send before call SDL_SetAppEventHandler at base_instance.
				break;
			case SDL_APP_TERMINATING:
			case SDL_APP_WILLENTERBACKGROUND:
			case SDL_APP_DIDENTERBACKGROUND:
			case SDL_QUIT:
				VALIDATE(false, "this event should be processed by SDL_SetAppEventHandler.");
				break;

			case SDL_APP_LOWMEMORY:
				instance->handle_app_event(event.type);
				break;

			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_APP_TERMINATING || event.window.event == SDL_APP_WILLENTERBACKGROUND || event.window.event == SDL_APP_DIDENTERBACKGROUND) {
					VALIDATE(false, "this event should be processed by SDL_SetWindowEventHandler.");
				}
				if (event.window.event == SDL_WINDOWEVENT_ENTER || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
					cursor::set_focus(true);

				} else if (event.window.event == SDL_WINDOWEVENT_LEAVE || event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
					cursor::set_focus(false);

				} else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
					// if the window must be redrawn, update the entire screen

				} else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					
				}

				// once window event, cancel longpress.
				longpress_event.timestamp = 0;

				break;

			case SDL_MOUSEMOTION: {
				//always make sure a cursor is displayed if the
				//mouse moves or if the user clicks
				cursor::set_focus(true);
				if (longpress_event.timestamp) {
					int abs_x = abs(event.motion.x - longpress_down.x);
					int abs_y = abs(event.motion.y - longpress_down.y);
					if (abs_x >= gui2::settings::clear_click_threshold || abs_y >= gui2::settings::clear_click_threshold) {
						longpress_event.timestamp = 0;
					} else {
						// update longpress's coordinate
						longpress_event.data1 = reinterpret_cast<void*>(event.motion.x);
						longpress_event.data2 = reinterpret_cast<void*>(event.motion.y);
					}
				}
				break;
			}
					  
			case SDL_MOUSEBUTTONDOWN: {
				cursor::set_focus(true);
				if (event.button.button == SDL_BUTTON_LEFT && event.button.clicks == 1) {
					longpress_event.timestamp = SDL_GetTicks();
					longpress_down = tpoint(event.button.x, event.button.y);
					longpress_event.data1 = reinterpret_cast<void*>(event.button.x);
					longpress_event.data2 = reinterpret_cast<void*>(event.button.y);
				} else {
					longpress_event.timestamp = 0;
				}
				break;
			}

			case SDL_MOUSEBUTTONUP:
				longpress_event.timestamp = 0;
				break;

#if defined(_X11) && !defined(__APPLE__)
			case SDL_SYSWMEVENT: {
				//clipboard support for X11
				handle_system_event(event);
				break;
			}
#endif
		}

		const std::vector<tevent_dispatcher*>& event_handlers = contexts.handlers;

		// when tow handler exists together, must first is base_controller, second is gui2::event::thandler. 
		// change resolution and resize window maybe make second dirty.
		for (size_t i1 = 0, i2 = event_handlers.size(); i1 != i2 && i1 < event_handlers.size(); ++i1) {
			event_handlers[i1]->handle_event(event);
			if (display::require_change_resolution) {
				display* disp = display::get_singleton();
				if (disp) {
					disp->change_resolution();
				}
			}
		}

	}

	instance->pump();

	// inform the pump monitors that an events::pump() has occurred
	for (size_t i1 = 0, i2 = pump_monitors.size(); i1 != i2 && i1 < pump_monitors.size(); ++i1) {
		pump_monitors[i1]->monitor_process();
	}
}

int discard(Uint32 event_mask_min, Uint32 event_mask_max)
{
	int discard_count = 0;
	SDL_Event temp_event;
	std::vector< SDL_Event > keepers;
	SDL_Delay(10);
	while (SDL_PollEvent(&temp_event) > 0) {
		if (temp_event.type >= event_mask_min && temp_event.type <= event_mask_max) {
			keepers.push_back( temp_event );
		} else {
			++ discard_count;
		}
	}

	//FIXME: there is a chance new events are added before kept events are replaced
	for (unsigned int i = 0; i < keepers.size(); ++i) {
		if (SDL_PushEvent(&keepers[i]) <= 0) {
			SDL_Log("failed to return an event to the queue.");
		}
	}

	return discard_count;
}

} //end events namespace
