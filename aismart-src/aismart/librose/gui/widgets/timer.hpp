/* $Id: timer.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

/**
 * @file
 * Contains the gui2 timer routines.
 *
 * This code avoids the following problems with the sdl timers:
 * - the callback must be a C function with a fixed signature.
 * - the callback needs to push an event in the event queue, between the
 *   pushing and execution of that event the timer can't be stopped. (Makes
 *   sense since the timer has expired, but not what the user wants.)
 *
 * With these functions it's possible to remove the event between pushing in
 * the queue and the actual execution. Since the callback is a boost::function
 * object it's possible to make the callback as fancy as wanted.
 */

#ifndef GUI_WIDGETS_TIMER_HPP_INCLUDED
#define GUI_WIDGETS_TIMER_HPP_INCLUDED

#include <boost/function.hpp>
#include <SDL_types.h>
#include <SDL_timer.h>
#include <map>

#define INVALID_TIMER_ID		0
namespace gui2 {

class twindow;

class ttimer
{
	friend twindow;

public:
	ttimer()
		: id(INVALID_TIMER_ID)
		, executing_(false)
	{}
	~ttimer()
	{
		reset();
	}

	/**
	 * Adds a new timer.
	 *
	 * @param interval                The timer interval in ms.
	 * @param callback                The function to call when the timer expires,
	 *                                the id send as parameter is the id of the
	 *                                timer.
	 * @param top_only                If true the timer will execute only when window is topest in Z axis.
	 *
	 */
	void reset(const Uint32 interval, twindow& window, const boost::function<void(size_t id)>& callback, bool top_only = true);
	void reset();
	bool valid() const { return id != INVALID_TIMER_ID; }

private:
	void reset_timer2();
	void execute();

public:
	class texecutor
	{
	public:
		texecutor(ttimer& timer, size_t id);
		~texecutor();

	private:
		ttimer& timer_;	
	};

	struct ttimer2
	{
		ttimer2(const twindow& window, bool top_only)
			: window(window)
			, top_only(top_only)
			, sdl_id(0)
			, interval(0)
			, callback()
		{
		}

		const twindow& window;
		const bool top_only;

		SDL_TimerID sdl_id;
		Uint32 interval;
		boost::function<void(size_t id)> callback;
	};
	std::unique_ptr<ttimer2> timer2; // app don't use timer2.

	static size_t sid;
	size_t id;
	bool executing_;
};


} //namespace gui2

#endif

