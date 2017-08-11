/* $Id: preferences_display.hpp 46186 2010-09-01 21:12:38Z silene $ */
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

#ifndef PREFERENCES_DISPLAY_HPP_INCLUDED
#define PREFERENCES_DISPLAY_HPP_INCLUDED

class CVideo;
class config;

#include <string>

namespace preferences {

	// this result maybe return by tdialog. twindow::OK is 0, so must large than 0.
	// reserve by other value, start at 100.
	enum tresoluton {CHANGE_RESOLUTION = 100, MIN_RESOLUTION = CHANGE_RESOLUTION, MAKE_FULLSCREEN, MAKE_WINDOWED, MAX_RESOLUTION = MAKE_WINDOWED};

	void set_fullscreen(CVideo& video, const bool ison);
	void set_scroll_to_action(bool ison);

	/**
	 * Set the resolution.
	 *
	 * @param video               The video 'holding' the framebuffer.
	 * @param width               The new width.
	 * @param height              The new height.
	 *
	 * @returns                   The status true if width and height are the
	 *                            size of the framebuffer, false otherwise.
	 */
	bool set_resolution(CVideo& video, const unsigned width, const unsigned height, bool scene = true);
	void set_grid(bool ison);

	// Control unit idle animations
	void set_default_move(bool ison);

	bool show_resolution_dialog(CVideo& video);
	void show_preferences_dialog(CVideo& video);
	bool is_resolution_retval(int res);

} // end namespace preferences

#endif
