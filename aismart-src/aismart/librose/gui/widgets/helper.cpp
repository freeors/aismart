/* $Id: helper.cpp 54413 2012-06-15 18:30:47Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/helper.hpp"

#include "gui/widgets/settings.hpp"

#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "SDL_ttf.h"

namespace gui2 {

bool init() 
{
	VALIDATE(!settings::actived, null_str);

	load_settings();

	return settings::actived;
}

SDL_Rect create_rect(const tpoint& origin, const tpoint& size)
{
	return ::create_rect(origin.x, origin.y, size.x, size.y);
}

unsigned decode_font_style(const std::string& style)
{
	if(style == "bold") {
		return TTF_STYLE_BOLD;
	} else if(style == "italic") {
		return TTF_STYLE_ITALIC;
	} else if(style == "underline") {
		return TTF_STYLE_UNDERLINE;
	} else if(style.empty() || style == "normal") {
		return TTF_STYLE_NORMAL;
	}

	// Unknown style 'style' using 'normal' instead.

	return TTF_STYLE_NORMAL;
}

t_string missing_widget(const std::string& id)
{
	utils::string_map symbols;
	symbols["id"] = id;

	return t_string(vgettext2("Mandatory widget '$id' hasn't been defined.", symbols));
}

void get_screen_size_variables(game_logic::map_formula_callable& variable)
{
	variable.add("screen_width", variant(settings::screen_width / twidget::hdpi_scale));
	variable.add("screen_height", variant(settings::screen_height / twidget::hdpi_scale));
	variable.add("svga", variant(game_config::svga));
	variable.add("vga", variant((int)settings::screen_width >= 640 * twidget::hdpi_scale && (int)settings::screen_height >= 480 * twidget::hdpi_scale));
	variable.add("statusbar_height", variant(game_config::statusbar_visible? game_config::statusbar_height / twidget::hdpi_scale: 0));
}

game_logic::map_formula_callable get_screen_size_variables()
{
	game_logic::map_formula_callable result;
	get_screen_size_variables(result);

	return result;
}

tpoint get_mouse_position()
{
	int x, y;
	SDL_GetMouseState(&x, &y);

	return tpoint(x, y);
}

} // namespace gui2

