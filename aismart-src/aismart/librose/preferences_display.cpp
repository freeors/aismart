/* $Id: preferences_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#define GETTEXT_DOMAIN "rose-lib"

/**
 *  @file
 *  Manage display-related preferences, e.g. screen-size, etc.
 */

#include "global.hpp"
#include "preferences_display.hpp"

#include "base_instance.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/window.hpp"
#include "display.hpp"

#include <boost/foreach.hpp>

namespace preferences {

void set_fullscreen(CVideo& video, const bool ison)
{
	_set_fullscreen(ison);

	const std::pair<int,int>& res = resolution(video);
	if (video.isFullScreen() != ison) {
		const int flags = ison ? SDL_WINDOW_FULLSCREEN: 0;
		int bpp = video.modePossible(res.first, res.second, 32, flags);
		VALIDATE(bpp > 0, "bpp must be large than 0!");

		// tpoint orientation_size = gui2::twidget::orientation_swap_size(res.first, res.second);

		video.setMode(res.first, res.second, flags);
		display::require_change_resolution = true;
	}
}

void set_scroll_to_action(bool ison)
{
	_set_scroll_to_action(ison);
}

// width, height are consistent with current orientation.
bool set_resolution(CVideo& video, const unsigned width, const unsigned height, bool scene)
{
	// Make sure resolutions are always divisible by 4
	//res.first &= ~3;
	//res.second &= ~3;

	SDL_Rect rect = screen_area();
	if (rect.w == width && rect.h == height) {
		return true;
	}

	tpoint landscape_size = gui2::twidget::orientation_swap_size(width, height);

	if (landscape_size.x < preferences::min_allowed_width() || landscape_size.y < preferences::min_allowed_height()) {
		return false;
	}

	const int flags = fullscreen() ? SDL_WINDOW_FULLSCREEN : 0;
	int bpp = video.modePossible(width, height, 32, flags);
	VALIDATE(bpp > 0, "bpp must be large than 0!");

	video.setMode(width, height, flags);
	if (scene) {
		display::require_change_resolution = true;
	}

	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	preferences::set('x' + postfix, lexical_cast<std::string>(landscape_size.x / gui2::twidget::hdpi_scale));
	preferences::set('y' + postfix, lexical_cast<std::string>(landscape_size.y / gui2::twidget::hdpi_scale));
	
	return true;
}

void set_grid(bool ison)
{
	VALIDATE(!display::get_singleton(), null_str);
	_set_grid(ison);
}

void set_default_move(bool ison) 
{
	_set_default_move(ison);
}

bool compare_resolutions(const std::pair<int,int>& lhs, const std::pair<int,int>& rhs)
{
	return lhs.first*lhs.second < rhs.first*rhs.second;
}

bool show_resolution_dialog(CVideo& video)
{
	bool fullScreen = fullscreen();

	std::vector<std::pair<int,int> > resolutions;
	int display = SDL_GetWindowDisplayIndex(video.getWindow());
	int modes = SDL_GetNumDisplayModes(display);
	SDL_DisplayMode mode = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
	SDL_Rect screen_rect = video.bound();
	for (int n = 0; n < modes; n ++) {
		SDL_GetDisplayMode(display, n, &mode);
		if (SDL_BYTESPERPIXEL(mode.format) != SDL_BYTESPERPIXEL(video.getformat().format)) {
			continue;
		}
		if (mode.w % gui2::twidget::hdpi_scale || mode.h % gui2::twidget::hdpi_scale) {
			continue;
		}
		if (!fullScreen && (mode.w >= screen_rect.w || mode.h >= screen_rect.h)) {
			continue;
		}
		if (mode.w >= min_allowed_width() && mode.h >= min_allowed_height()) {
			resolutions.push_back(std::pair<int, int>(mode.w, mode.h));
		}
	}

	if (resolutions.empty()) {
		gui2::show_transient_message("", _("There are no alternative video modes available"));
		return false;
	}

	tpoint landscape_size = gui2::twidget::orientation_swap_size(video.getx(), video.gety());
	const std::pair<int,int> current_res(landscape_size.x, landscape_size.y);
	resolutions.push_back(current_res);
	if (!fullScreen) {
		resolutions.push_back(std::make_pair(480 * gui2::twidget::hdpi_scale, 320 * gui2::twidget::hdpi_scale));
		resolutions.push_back(std::make_pair(568 * gui2::twidget::hdpi_scale, 320 * gui2::twidget::hdpi_scale));
		resolutions.push_back(std::make_pair(640 * gui2::twidget::hdpi_scale, 360 * gui2::twidget::hdpi_scale));
		resolutions.push_back(std::make_pair(667 * gui2::twidget::hdpi_scale, 375 * gui2::twidget::hdpi_scale));
	}

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<std::string> options;
	int current_choice = nposm;

	for (size_t k = 0; k < resolutions.size(); ++k) {
		std::pair<int, int> const& res = resolutions[k];
		std::ostringstream option;

		if (res == current_res) {
			current_choice = static_cast<int>(k);		
		}

		option << res.first << "x" << res.second;
		// widescreen threshold is 16:10
		if ((double)res.first/res.second >= 16.0/10.0) {
			option << _(" (widescreen)");
		}
		options.push_back(option.str());
	}

	int choice = nposm;
	if (game_config::svga) {
		gui2::tcombo_box dlg(options, current_choice, _("Choose Resolution"), true);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}
		choice = dlg.cursel();

	} else {
		gui2::tcombo_box2 dlg(_("Choose Resolution"), options, current_choice);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return false;
		}
		choice = dlg.cursel();
	}
	VALIDATE(choice != nposm, null_str);
	if (resolutions[choice] == current_res) {
		return false;
	}

	std::pair<int, int>& res = resolutions[choice];

	tpoint normal_size = gui2::twidget::orientation_swap_size(res.first, res.second);
	set_resolution(video, normal_size.x, normal_size.y);
	return true;
}

void show_preferences_dialog(CVideo& video)
{
	int res = instance->show_preferences_dialog();
	if (res == preferences::CHANGE_RESOLUTION) {
		preferences::show_resolution_dialog(video);

	} else if (res == preferences::MAKE_FULLSCREEN) {
		preferences::set_fullscreen(video, true);

	} else if (res == preferences::MAKE_WINDOWED) {
		preferences::set_fullscreen(video, false);

	}
}

bool is_resolution_retval(int res) 
{ 
	return res >= MIN_RESOLUTION && res <= MAX_RESOLUTION; 
}

} // end namespace preferences

