/* $Id: base_controller.cpp 47506 2010-11-07 20:19:57Z silene $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "base_controller.hpp"

#include "gui/auxiliary/event/handler.hpp"
#include "gui/widgets/window.hpp"
#include "display.hpp"
#include "base_map.hpp"
#include "preferences.hpp"
#include "mouse_handler_base.hpp"
#include "hotkeys.hpp"
#include "sound.hpp"

base_controller::base_controller(int ticks, const config& app_cfg, CVideo& video)
	: tevent_dispatcher(true, true)
	, app_cfg_(app_cfg)
	, video_(video)
	, ticks_(ticks)
	, drag_from_(construct_null_coordinate())
	, mouseover_unit_(nullptr)
	, key_()
	, browse_(false)
	, scrolling_(false)
	, finger_motion_scroll_(false)
	, finger_motion_direction_(UP)
	, do_quit_(false)
	, quit_mode_(gui2::twindow::OK)
{
}

base_controller::~base_controller()
{
}

void base_controller::initialize(int initial_zoom)
{
	VALIDATE(initial_zoom > 0 && !(initial_zoom & 0x3), null_str);
	init_music(app_cfg_);

	app_create_display(initial_zoom);

	display& disp = get_display();
	VALIDATE(initial_zoom >= disp.min_zoom() && initial_zoom <= disp.max_zoom(), null_str);

	disp.initialize();

	app_post_initialize();
}

int base_controller::main_loop()
{
	try {
		while (!do_quit_) {
			play_slice();
		}
	} catch (twml_exception& e) {
		e.show();
	}
	return quit_mode_;
}

void base_controller::init_music(const config& game_config)
{
	const config &cfg = game_config.child("chart_music");
	if (!cfg) {
		return;
	}
	BOOST_FOREACH (const config &i, cfg.child_range("music")) {
		sound::play_music_config(i);
	}
	sound::commit_music_changes();
}

bool base_controller::finger_coordinate_valid(int x, int y) const
{
	const display& disp = get_display();

	if (!point_in_rect(x, y, disp.main_map_widget_rect())) {
		return false;
	}
	if (disp.point_in_volatiles(x, y)) {
		return false;
	}
	return true;
}

bool base_controller::mouse_wheel_coordinate_valid(int x, int y) const
{
	const display& disp = get_display();

	return point_in_rect(x, y, disp.main_map_widget_rect());
}

void base_controller::pinch_event(bool out)
{
	display& disp = get_display();
/*
	int increase = disp.zoom() / 2;
	increase &= ~0x3;
*/
	int increase = 8;
	if (!increase) {
		return;
	}

	if (out) {
		disp.set_zoom(-1 * increase);
	} else {
		disp.set_zoom(increase);
	}
}

void base_controller::handle_swipe(const int x, const int y, const int xlevel, const int ylevel)
{
	int abs_xlevel = abs(xlevel);
	int abs_ylevel = abs(ylevel);

	if (abs_xlevel >= swipe_wheel_level_gap) {
		VALIDATE(abs_xlevel > swipe_wheel_level_gap, null_str);
		abs_xlevel -= swipe_wheel_level_gap;
	}
	if (abs_ylevel >= swipe_wheel_level_gap) {
		VALIDATE(abs_ylevel > swipe_wheel_level_gap, null_str);
		abs_ylevel -= swipe_wheel_level_gap;
	}

	if (abs_xlevel && abs_ylevel) {
		if (xlevel > 0) {
			if (ylevel > 0) {
				finger_motion_direction_ = SOUTH_EAST;
			} else {
				finger_motion_direction_ = NORTH_EAST;
			}
		} else if (ylevel >= 0) {
			finger_motion_direction_ = SOUTH_WEST;
		} else {
			finger_motion_direction_ = NORTH_WEST;
		}
		finger_motion_scroll_ = true;
	} else if (abs_xlevel) {
		// x axis
		if (xlevel > 0) {
			finger_motion_direction_ = RIGHT;
		} else {
			finger_motion_direction_ = LEFT;
		}
		finger_motion_scroll_ = true;
	} else {
		// y axis
		if (ylevel > 0) {
			finger_motion_direction_ = DOWN;
		} else {
			finger_motion_direction_ = UP;
		}
		finger_motion_scroll_ = true;
	}
}

void base_controller::handle_pinch(const int x, const int y, const bool out)
{
	pinch_event(out);
}

void base_controller::handle_mouse_motion(const SDL_MouseMotionEvent& motion)
{
	// (x, y) on volatile control myabe in map, don't check in volatiles.

	display& disp = get_display();
	
	events::mouse_handler_base& mouse_handler = get_mouse_handler_base();
	bool minimap = mouse_handler.mouse_motion_default(motion.x, motion.y);

	// recalculate mouse_over_unit and disp::mouse_over_hex
	mouseover_unit_ = nullptr;
	map_location mouseover_hex;
	if (!minimap) {
		const base_map& units = get_units();
		mouseover_unit_ = units.find_base_unit(motion.x, motion.y);
		mouseover_hex = disp.screen_2_loc(motion.x, motion.y);
	}
	disp.set_mouseover_hex(mouseover_hex);

	// call app_mouse_motion
	bool allow_scroll = app_mouse_motion(motion.x, motion.y, minimap);
	if (!minimap && allow_scroll && (motion.xrel || motion.yrel) && mouse_handler.is_dragging_left()) {
		if (!is_null_coordinate(drag_from_)) {
			disp.scroll(-1 * motion.xrel, -1 * motion.yrel);
		}
	}
}

void base_controller::handle_mouse_down(const SDL_MouseButtonEvent& button)
{
	display& disp = get_display();
	if (disp.point_in_volatiles(button.x, button.y)) {
		return;
	}

	// user maybe want to click at _mini_map. so allow click out of main-map.
	events::mouse_handler_base& mouse_handler = get_mouse_handler_base();
	mouse_handler.mouse_press(button);

	if (mouse_handler.is_left_click(button)) {
		// i can not keep down/up pare.
		set_null_coordinate(drag_from_);

		const bool minimap = mouse_handler.minimap_left_middle_down(button.x, button.y);
		if (!minimap && !disp.screen_2_loc(button.x, button.y).valid()) { 
			return;
		}
		drag_from_.x = button.x;
		drag_from_.y = button.y;
		app_left_mouse_down(button.x, button.y, minimap);

	} else if (mouse_handler.is_right_click(button)) {
		if (!disp.screen_2_loc(button.x, button.y).valid()) { 
			return;
		}
		app_right_mouse_down(button.x, button.y);

	}

	if (get_mouse_handler_base().get_show_menu()) {
		disp.goto_main_context_menu();
	}
}

void base_controller::handle_mouse_up(const SDL_MouseButtonEvent& button)
{
	display& disp = get_display();
	// user maybe want to click at _mini_map. so allow click out of main-map.

	events::mouse_handler_base& mouse_handler = get_mouse_handler_base();
	mouse_handler.mouse_press(button);

	if (mouse_handler.is_left_click(button)) {
		bool click = !multi_gestures() && !mouse_motions_ && !is_magic_coordinate(button) && !is_null_coordinate(drag_from_);
		app_left_mouse_up(button.x, button.y, click);
		set_null_coordinate(drag_from_);

	} else if (mouse_handler.is_right_click(button)) {
		app_right_mouse_up(button.x, button.y);
	}

	if (get_mouse_handler_base().get_show_menu()){
		disp.goto_main_context_menu();
	}
}

void base_controller::handle_mouse_leave_main_map(const int x, const int y, const int up_result)
{
	app_mouse_leave_main_map(x, y, up_result);
}

bool base_controller::mini_pre_handle_event(const SDL_Event& event)
{
	if (gui2::is_in_dialog()) {
		return false;
	}
	return true;
}

void base_controller::mini_handle_event(const SDL_Event& event)
{
	switch(event.type) {
	case SDL_KEYDOWN:
		// Detect key press events, unless there something that has keyboard focus
		// in which case the key press events should go only to it.
		if (have_keyboard_focus()) {
			process_keydown_event(event);
			// hotkey::key_event(get_display(),event.key,this);
		} else {
			process_focus_keydown_event(event);
			break;
		}
		// intentionally fall-through
	case SDL_KEYUP:
		process_keyup_event(event);
		break;

	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
			mouse_leave_window();

			events::mouse_handler_base& mouse_handler = get_mouse_handler_base();

			SDL_MouseButtonEvent up;
			up.type = SDL_MOUSEBUTTONUP;
			up.state = SDL_RELEASED;
			set_mouse_leave_window_event(up);
			
			std::vector<int> buttons;
			buttons.push_back(SDL_BUTTON_LEFT);
			buttons.push_back(SDL_BUTTON_MIDDLE);
			buttons.push_back(SDL_BUTTON_RIGHT);

			for (std::vector<int>::const_iterator it = buttons.begin(); it != buttons.end(); ++ it) {
				up.button = *it;
				handle_mouse_up(up);
			}
		}
		break;

	default:
		break;
	}
}

bool base_controller::have_keyboard_focus()
{
	return true;
}

void base_controller::process_focus_keydown_event(const SDL_Event& /*event*/) {
	//no action by default
}

void base_controller::process_keydown_event(const SDL_Event& /*event*/) {
	//no action by default
}

void base_controller::process_keyup_event(const SDL_Event& /*event*/) {
	//no action by default
}

#define SDL_APPMOUSEFOCUS	0x01
#define SDL_APPINPUTFOCUS	0x02
#define SDL_APPACTIVE		0x04

static Uint8 SDL_GetAppState(SDL_Window* window)
{
    Uint8 state = 0;
    Uint32 flags = 0;

	flags = SDL_GetWindowFlags(window);
	// as if minimized into task_bar of windows, flags include SDL_WINDOW_SHOWN.
	// in order to cut down consume, I think minimized app isn't in active.
    if ((flags & SDL_WINDOW_SHOWN) && !(flags & SDL_WINDOW_MINIMIZED)) {
        state |= SDL_APPACTIVE;
    }
    if (flags & SDL_WINDOW_INPUT_FOCUS) {
        state |= SDL_APPINPUTFOCUS;
    }
    if (flags & SDL_WINDOW_MOUSE_FOCUS) {
        state |= SDL_APPMOUSEFOCUS;
    }
    return state;
}

bool base_controller::handle_scroll(CKey& key, int mousex, int mousey, int mouse_flags)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	// for tablet device.
	bool mouse_in_window = false;
#else
	// for mouse device.
	bool mouse_in_window = (SDL_GetAppState(get_display().video().getWindow()) & SDL_APPMOUSEFOCUS) != 0;
#endif
	
	bool keyboard_focus = have_keyboard_focus();
	int scroll_speed = preferences::scroll_speed() * gui2::twidget::hdpi_scale;
	int dx = 0, dy = 0;
	int scroll_threshold = (preferences::mouse_scroll_enabled())? preferences::mouse_scroll_threshold() : 0;
	if ((key[SDLK_UP] && keyboard_focus) || (mousey < scroll_threshold && mouse_in_window)) {
		dy -= scroll_speed;
	}
	if ((key[SDLK_DOWN] && keyboard_focus) || (mousey > get_display().h() - scroll_threshold && mouse_in_window)) {
		dy += scroll_speed;
	}
	if ((key[SDLK_LEFT] && keyboard_focus) || (mousex < scroll_threshold && mouse_in_window)) {
		dx -= scroll_speed;
	}
	if ((key[SDLK_RIGHT] && keyboard_focus) || (mousex > get_display().w() - scroll_threshold && mouse_in_window)) {
		dx += scroll_speed;
	}
	if ((mouse_flags & SDL_BUTTON_MMASK) != 0 && preferences::middle_click_scrolls()) {
		const SDL_Rect& rect = get_display().main_map_widget_rect();
		if (point_in_rect(mousex, mousey,rect)) {
			// relative distance from the center to the border
			// NOTE: the view is a rectangle, so can be more sensible in one direction
			// but seems intuitive to use and it's useful since you must
			// more often scroll in the direction where the view is shorter
			const double xdisp = ((1.0*mousex / rect.w) - 0.5);
			const double ydisp = ((1.0*mousey / rect.h) - 0.5);
			// 4.0 give twice the normal speed when mouse is at border (xdisp=0.5)
			int speed = 4 * scroll_speed;
			dx += round_double(xdisp * speed);
			dy += round_double(ydisp * speed);
		}
	}
	if (finger_motion_scroll_) {
		if (finger_motion_direction_ == DOWN) {
			dy -= scroll_speed;
		} else if (finger_motion_direction_ == UP) {
			dy += scroll_speed;
		} else if (finger_motion_direction_ == RIGHT) {
			dx -= scroll_speed;
		} else if (finger_motion_direction_ == LEFT) {
			dx += scroll_speed;
		} else if (finger_motion_direction_ == SOUTH_WEST) {
			dx += scroll_speed;
			dy -= scroll_speed;
		} else if (finger_motion_direction_ == NORTH_WEST) {
			dx += scroll_speed;
			dy += scroll_speed;
		} else if (finger_motion_direction_ == NORTH_EAST) {
			dx -= scroll_speed;
			dy += scroll_speed;
		} else if (finger_motion_direction_ == SOUTH_EAST) {
			dx -= scroll_speed;
			dy -= scroll_speed;
		} 
		finger_motion_scroll_ = false;
	}
	if (dx || dy) {
		return get_display().scroll(dx, dy);
	}
	return false;
}

void base_controller::play_slice(bool delay_enabled)
{
	display& gui = get_display();

	CKey key;
	events::pump();

	slice_before_scroll();

	int mousex, mousey;
	Uint8 mouse_flags = SDL_GetMouseState(&mousex, &mousey);
	bool was_scrolling = scrolling_;
	scrolling_ = handle_scroll(key, mousex, mousey, mouse_flags);

	gui.draw();

	// be nice when window is not visible
	// NOTE should be handled by display instead, to only disable drawing
	if (delay_enabled && (SDL_GetAppState(video_.getWindow()) & SDL_APPACTIVE) == 0) {
		gui.delay(200);
	}

	if (!scrolling_ && was_scrolling) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	
#else
		// scrolling ended, update the cursor and the brightened hex
		int x, y;
		SDL_GetMouseState(&x,&y);

		SDL_MouseMotionEvent motion;
		motion.x = x;
		motion.y = y;
		motion.xrel = 0;
		motion.yrel = 0;
		handle_mouse_motion(motion);
#endif
	}
	slice_end();
}

void base_controller::slice_before_scroll()
{
	//no action by default
}

void base_controller::slice_end()
{
	//no action by default
}

bool base_controller::actived_context_menu(const std::string& id) const
{ 
	const display& disp = get_display();
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	int zoom = disp.hex_width();

	switch (command) {
	case HOTKEY_ZOOM_IN:
		return zoom < disp.max_zoom();
	case HOTKEY_ZOOM_OUT:
		return zoom > disp.min_zoom();
	}

	return true; 
}

void base_controller::execute_command(int command, const std::string& sparam)
{
	if (!app_can_execute_command(command, sparam)) {
		return;
	}
	return app_execute_command(command, sparam);
}

void base_controller::app_execute_command(int command, const std::string& sparam)
{
	display& disp = get_display();

	switch(command) {
	case HOTKEY_SYSTEM:
		// system();
		return;

	case HOTKEY_ZOOM_IN:
		disp.set_zoom(ZOOM_INCREMENT);
		return;

	case HOTKEY_ZOOM_OUT:
		disp.set_zoom(-ZOOM_INCREMENT);
		return;

	case HOTKEY_ZOOM_DEFAULT:
		disp.set_default_zoom();
		return;
	}
}

