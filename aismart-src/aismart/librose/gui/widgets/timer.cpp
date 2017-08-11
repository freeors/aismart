/* $Id: timer.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "gui/widgets/timer.hpp"
#include "gui/widgets/window.hpp"

#include "events.hpp"
#include "wml_exception.hpp"

#include <SDL_timer.h>
#include <map>

namespace gui2 {


	/** Ids for the timers. */
	// static size_t id = 0;

	/** The active timers. */
	static std::map<size_t, ttimer*> timers;

	/** The id of the event being executed, 0 if none. */
	static size_t executing_id = 0;

	/** Did somebody try to remove the timer during its execution? */
	static bool executing_id_removed = false;

/**
 * Helper to make removing a timer in a callback safe.
 *
 * Upon creation it sets the executing id and clears the remove request flag.
 *
 * If an remove_timer() is called for the id being executed it requests a
 * remove the timer and exits remove_timer().
 *
 * Upon destruction it tests whether there was a request to remove the id and
 * does so. It also clears the executing id. It leaves the remove request flag
 * since the execution function needs to know whether or not the event was
 * removed.
 */
ttimer::texecutor::texecutor(ttimer& timer, size_t id)
	: timer_(timer)
{
	VALIDATE(!timer_.executing_, null_str);
	timer_.executing_ = true;

	executing_id = id;
	executing_id_removed = false;
}

ttimer::texecutor::~texecutor()
{
	const size_t id = executing_id;
	executing_id = 0;
	if (executing_id_removed) {
		std::map<size_t, ttimer*>::iterator itor = timers.find(id);
		VALIDATE(itor != timers.end(), null_str);
		itor->second->reset();
	}

	timer_.executing_ = false;
}

extern "C" {

static Uint32 timer_callback(Uint32, void* param)
{
	const ttimer& timer = *reinterpret_cast<ttimer*>(param);
	if (!timers.count(timer.id)) {
		return 0;
	}

	SDL_Event event;
	SDL_UserEvent data;

	data.type = TIMER_EVENT;
	data.code = 0;
	data.data1 = reinterpret_cast<void*>(timer.id);
	data.data2 = param;

	event.type = TIMER_EVENT;
	event.user = data;

	SDL_PushEvent(&event);

	return timer.timer2->interval;
}

} // extern "C"

size_t ttimer::sid = 0;

void ttimer::reset_timer2()
{
	VALIDATE(id != INVALID_TIMER_ID && timer2.get(), null_str);
	id = INVALID_TIMER_ID;
	timer2.reset();
}

void ttimer::reset()
{
	if (id == INVALID_TIMER_ID) {
		VALIDATE(!timer2.get(), null_str);
		return;
	}

	std::map<size_t, ttimer*>::iterator itor = timers.find(id);
	if (itor == timers.end()) {
		// Can't remove timer since it no longer exists.
		return;
	}
	VALIDATE(itor->second == this, null_str);

	if (id == executing_id) {
		executing_id_removed = true;
		return;
	}

	if (!SDL_RemoveTimer(timer2->sdl_id)) {
		/*
		 * This can happen if the caller of the timer didn't get the event yet
		 * but the timer has already been fired. This due to the fact that a
		 * timer pushes an event in the queue, which allows the following
		 * condition:
		 * - Timer fires
		 * - Push event in queue
		 * - Another event is processed and tries to remove the event.
		 */
		// The timer is already out of the SDL timer list.
	}
	reset_timer2();
	timers.erase(itor);
}

void ttimer::reset(const Uint32 interval, twindow& window, const boost::function<void(size_t id)>& callback, bool top_only)
{
	reset();

	do {
		++ sid;
	} while (sid == 0 || timers.find(sid) != timers.end());

	id = sid;
	timer2.reset(new ttimer2(window, top_only));
	std::pair<std::map<size_t, ttimer*>::iterator, bool> ins = timers.insert(std::make_pair(sid, this));

	timer2->sdl_id = SDL_AddTimer(interval, timer_callback, reinterpret_cast<void*>(this));
	VALIDATE(timer2->sdl_id != 0, null_str); // Failed to create an sdl timer.

	// always repeat. app should call ttimer.reset() to remove timer.
	timer2->interval = interval;
	timer2->callback = callback;
}

void ttimer::execute()
{
	VALIDATE(valid(), null_str);
	if (executing_) {
		// avoid reenter.
		return;
	}

	texecutor executor(*this, id);
	timer2->callback(id);
}

void twindow::remove_timer_during_destructor()
{
	for (std::map<size_t, ttimer*>::iterator it = timers.begin(); it != timers.end(); ) {
		ttimer& timer = *it->second;
		const ttimer::ttimer2& timer2 = *timer.timer2;
		const twindow& window2 = timer2.window;
		if (&window2 != this) {
			++ it;
			continue;
		}
		VALIDATE(timer.id != executing_id, "removed timer during destructor must not be in executing.");

		if (!SDL_RemoveTimer(timer2.sdl_id)) {
			/*
			 * This can happen if the caller of the timer didn't get the event yet
			 * but the timer has already been fired. This due to the fact that a
			 * timer pushes an event in the queue, which allows the following
			 * condition:
			 * - Timer fires
			 * - Push event in queue
			 * - Another event is processed and tries to remove the event.
			 */
			// The timer is already out of the SDL timer list.
		}
		timer.reset_timer2();
		timers.erase(it ++);
	}
}

void twindow::execute_timer(void* data1, void* data2) const
{
	// this is top window in Z axis.
	const size_t id = reinterpret_cast<size_t>(data1);
	std::map<size_t, ttimer*>::iterator itor = timers.find(id);
	if (itor == timers.end()) {
		// this is timer is remove during interval.
		// Can't execute timer since it no longer exists.
		return;
	}

	ttimer& timer = *reinterpret_cast<ttimer*>(data2);
	const ttimer::ttimer2& timer2 = *timer.timer2.get();
	if (timer2.top_only && &timer2.window != this) {
		return;
	}

	timer.execute();
}

} //namespace gui2

