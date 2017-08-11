/* $Id: display.hpp 47673 2010-11-21 14:32:46Z mordante $ */
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

#ifndef HEX_DISPLAY_H_INCLUDED
#define HEX_DISPLAY_H_INCLUDED

#include "display.hpp"

class hex_display: public display
{
public:
	hex_display(base_controller& controller, CVideo& video, const tmap* map,
			size_t num_reports, int initial_zoom);

	/**
	 * Returns the maximum area used for the map
	 * regardless to resolution and view size
	 */
	const SDL_Rect& main_map_rect() const;

	/**
	 * Function which returns the width of a hex in pixels,
	 * up to where the next hex starts.
	 * (i.e. not entirely from tip to tip -- use zoom()
	 * to get the distance from tip to tip)
	 */
	int hex_width() const override { return (zoom_*3)/4; }

	const map_location map_2_loc(int mapx, int mapy) const;

	int loc_2_screen_x(const map_location& loc) const;
	int loc_2_screen_y(const map_location& loc) const;
	void loc_2_screen(const map_location& loc, int& x, int& y) const override;

	/** Return the rectangular area of hexes overlapped by r (r is in screen coordinates) */
	const rect_of_hexes hexes_under_rect(const SDL_Rect& r) const;

protected:
	void draw_border(const map_location& loc, const int xpos, const int ypos);
	double minimap_shift_x(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const;
	double minimap_shift_y(const SDL_Rect& map_rect, const SDL_Rect& map_out_rect) const;

};

#endif

