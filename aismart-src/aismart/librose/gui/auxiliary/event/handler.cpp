/* $Id: handler.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/auxiliary/event/handler.hpp"

#include "gui/auxiliary/event/dispatcher.hpp"
#include "gui/auxiliary/event/distributor.hpp"
#include "gui/widgets/helper.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/timer.hpp"
#include "hotkeys.hpp"
#include "video.hpp"
#include "area_anim.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/dialogs/dialog.hpp"
#include "preferences.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>

#include <cassert>


namespace gui2 {

tpoint revise_screen_size(const int width, const int height)
{
	tpoint landscape_size = twidget::orientation_swap_size(width, height);
	if (landscape_size.x < preferences::min_allowed_width()) {
		landscape_size.x = preferences::min_allowed_width();
	}
	if (landscape_size.y < preferences::min_allowed_height()) {
		landscape_size.y = preferences::min_allowed_height();
	}

	tpoint normal_size = twidget::orientation_swap_size(landscape_size.x, landscape_size.y);
	return normal_size;
}

namespace event {


/***** thandler class. *****/

/**
 * This singleton class handles all events.
 *
 * It's a new experimental class.
 */
class thandler: public tevent_dispatcher
{
	friend bool gui2::is_in_dialog();
	friend void gui2::absolute_draw();
	friend std::vector<twindow*> gui2::connectd_window();
	friend void gui2::clear_textures();
public:
	thandler();

	~thandler();

	/** Inherited from events::handler. */
	bool mini_pre_handle_event(const SDL_Event& event) override;
	void mini_handle_event(const SDL_Event& event) override;

	/**
	 * Connects a dispatcher.
	 *
	 * @param dispatcher              The dispatcher to connect.
	 */
	void connect(twindow* dispatcher);

	/**
	 * Disconnects a dispatcher.
	 *
	 * @param dispatcher              The dispatcher to disconnect.
	 */
	void disconnect(twindow* dispatcher);

private:
	// override tevent_dispatcher
	void handle_swipe(const int x, const int y, const int xlevel, const int ylevel) override;
	void handle_mouse_down(const SDL_MouseButtonEvent& button);
	void handle_mouse_up(const SDL_MouseButtonEvent& button);
	void handle_mouse_motion(const SDL_MouseMotionEvent& motion);
	void handle_longpress(const int x, const int y);

	/***** Handlers *****/

	/** Fires a draw event. */
	void draw();

	/**
	 * Fires a video resize event.
	 *
	 * @param new_size               The new size of the window.
	 */
	void video_resize(const tpoint& new_size);

	/**
	 * Fires a mouse button up event.
	 *
	 * @param position               The position of the mouse.
	 * @param button                 The SDL id of the button that caused the
	 *                               event.
	 */
	void mouse_button_up(const tpoint& position, const Uint8 button);

	/**
	 * Fires a mouse button down event.
	 *
	 * @param position               The position of the mouse.
	 * @param button                 The SDL id of the button that caused the
	 *                               event.
	 */
	void mouse_button_down(const tpoint& position, const Uint8 button);

	/**
	 * Gets the dispatcher that wants to receive the keyboard input.
	 *
	 * @returns                   The dispatcher.
	 * @retval NULL               No dispatcher found.
	 */
	tdispatcher* keyboard_dispatcher();

	/**
	 * Handles a hat motion event.
	 *
	 * @param event                  The SDL joystick hat event triggered.
	 */
	void hat_motion(const SDL_JoyHatEvent& event);


	/**
	 * Handles a joystick button down event.
	 *
	 * @param event                  The SDL joystick button event triggered.
	 */
	void button_down(const SDL_JoyButtonEvent& event);


	/**
	 * Fires a key down event.
	 *
	 * @param event                  The SDL keyboard event triggered.
	 */
	void key_down(const SDL_KeyboardEvent& event);

	/**
	 * Fires a text input event.
	 *
	 * @param event                  The SDL textinput event triggered.
	 */
	void text_input(const SDL_TextInputEvent& event);

	/**
	 * Handles the pressing of a hotkey.
	 *
	 * @param key                 The hotkey item pressed.
	 *
	 * @returns                   True if the hotkey is handled false otherwise.
	 */
	bool hotkey_pressed(const hotkey::hotkey_item& key);

	/**
	 * Fires a key down event.
	 *
	 * @param key                    The SDL key code of the key pressed.
	 * @param modifier               The SDL key modifiers used.
	 * @param unicode                The unicode value for the key pressed.
	 */
	void key_down(const SDL_Keycode key
			, const SDL_Keymod modifier
			, const Uint16 unicode);

private:
	/**
	 * The dispatchers.
	 *
	 * The order of the items in the list is also the z-order the front item
	 * being the one completely in the background and the back item the one
	 * completely in the foreground.
	 */
	std::vector<twindow*> dispatchers_;
};

static thandler* handler = NULL;

thandler::thandler()
	: tevent_dispatcher(false, false)
	, dispatchers_()
{
	if (SDL_WasInit(SDL_INIT_TIMER) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_TIMER) == -1) {
			assert(false);
		}
	}
}

thandler::~thandler()
{
}

void thandler::handle_swipe(const int x, const int y, const int xlevel, const int ylevel)
{
	if (dispatchers_.empty()) {
		return;
	}
	twindow& window = *dispatchers_.back();

	int abs_xlevel = abs(xlevel);
	int abs_ylevel = abs(ylevel);

	tevent event = WHEEL_RIGHT;
/*	if (abs_xlevel >= abs_ylevel) {
		// x axis
		if (xlevel > 0) {
			event = WHEEL_RIGHT;
		} else {
			event = WHEEL_LEFT;
		}

	} else {
		// y axis
		if (ylevel > 0) {
			event = WHEEL_DOWN;
		} else {
			event = WHEEL_UP;
		}
	}
*/
	if (!abs_ylevel) {
		return;
	}
	// now only tscroll_container use swipe, fixed in the future.
	{
		// y axis
		if (ylevel > 0) {
			event = WHEEL_DOWN;
		} else {
			event = WHEEL_UP;
		}
	}

	bool handled = false;
	window.distributor().signal_handler_sdl_wheel(event, handled, tpoint(x, y), tpoint(abs_xlevel, abs_ylevel));
}

void thandler::handle_mouse_down(const SDL_MouseButtonEvent& button)
{
	mouse_button_down(tpoint(button.x, button.y), button.button);
}

void thandler::handle_mouse_up(const SDL_MouseButtonEvent& button)
{
	mouse_button_up(tpoint(button.x, button.y), button.button);
}

void thandler::handle_mouse_motion(const SDL_MouseMotionEvent& motion)
{
	if (dispatchers_.empty()) {
		return;
	}
	twindow& window = *dispatchers_.back();
	if (window.is_closing()) {
		return;
	}

	bool handled = false;
	if (!is_mouse_leave_window_event(motion)) {
		handled = window.signal_handler_sdl_mouse_motion(tpoint(motion.x, motion.y));
	}
	if (!handled) {
		window.distributor().signal_handler_sdl_mouse_motion(tpoint(motion.x, motion.y), tpoint(motion.xrel, motion.yrel));
	}
}

void thandler::handle_longpress(const int x, const int y)
{
	if (dispatchers_.empty()) {
		return;
	}
	twindow& window = *dispatchers_.back();
	if (window.is_closing()) {
		return;
	}
	window.distributor().signal_handler_sdl_longpress(tpoint(x, y));
}

bool thandler::mini_pre_handle_event(const SDL_Event& event)
{
	if (dispatchers_.empty()) {
		return false;
	}
	return true;
}

void thandler::mini_handle_event(const SDL_Event& event)
{
	int x = 0, y = 0;

	switch(event.type) {
	case TIMER_EVENT:
		if (!dispatchers_.empty()) {
			dispatchers_.back()->execute_timer(event.user.data1, event.user.data2);
		}
		break;

	case SDL_JOYBUTTONDOWN:
		button_down(event.jbutton);
		break;

	case SDL_JOYBUTTONUP:
		break;

	case SDL_JOYAXISMOTION:
		break;

	case SDL_JOYHATMOTION:
		hat_motion(event.jhat);
		break;

	case SDL_KEYDOWN:
		key_down(event.key);
		break;

	case SDL_TEXTINPUT:
        text_input(event.text);
		break;

	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
			// Uint8 mouse_flags = SDL_GetMouseState(&x, &y);
			// SDL_Log("gui2::thandler, SDL_WINDOWEVENT_LEAVE, mouse_flags: 0x%08x\n", mouse_flags);
			// if (mouse_flags & SDL_BUTTON(SDL_BUTTON_LEFT)) {
				// mouse had leave window, simulate mouse motion and mouse up.
				mouse_leave_window();

				SDL_MouseMotionEvent motion;
				motion.type = SDL_MOUSEMOTION;
				set_mouse_leave_window_event(motion);
				motion.xrel = motion.yrel = 0;
				handle_mouse_motion(motion);
				if (twindow::eat_left_up) {
					SDL_Log("SDL_WINDOWEVENT_LEAVE, eat_left_up(%i) isn't 0, evalue to 0.\n", twindow::eat_left_up);
					twindow::eat_left_up = false;
				}
			// }

		} else if (event.window.event == SDL_WINDOWEVENT_EXPOSED) {
			// SDL_Log("SDL_WINDOWEVENT_EXPOSED\n");

		} else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
			// SDL_Log("SDL_WINDOWEVENT_RESIZED, %ix%i\n", event.window.data1, event.window.data2);
			video_resize(revise_screen_size(event.window.data1, event.window.data2));

		} else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
			// make sure safe, use tow reset. 1)focus_lost, 2)focus_gained.
			SDL_MouseMotionEvent motion;
			motion.type = SDL_MOUSEMOTION;
			set_focus_lost_event(motion);
			motion.xrel = motion.yrel = 0;
			handle_mouse_motion(motion);
			// SDL_Log("SDL_WINDOWEVENT_FOCUS_GAINED\n");

		}
		break;

#if defined(_X11) && !defined(__APPLE__)
	case SDL_SYSWMEVENT: {
		DBG_GUI_E << "Event: System event.\n";
		//clipboard support for X11
		handle_system_event(event);
		break;
	}
#endif

	// Silently ignored events.
	case SDL_KEYUP:
		break;

	default:
		break;
	}
}

void thandler::connect(twindow* window)
{
	VALIDATE(std::find(dispatchers_.begin(), dispatchers_.end(), window) == dispatchers_.end(), null_str);

	if (dispatchers_.empty()) {
		join();
	} else {
		dispatchers_.back()->window_connect_disconnected(true);
	}

	dispatchers_.push_back(window);
}

void thandler::disconnect(twindow* window)
{
	/***** Validate pre conditions. *****/
	std::vector<twindow*>::iterator itor = std::find(dispatchers_.begin(), dispatchers_.end(), window);
	VALIDATE(itor != dispatchers_.end(), null_str);

	/***** Remove dispatcher. *****/
	dispatchers_.erase(itor);

	/***** Validate post conditions. *****/
	VALIDATE(std::find(dispatchers_.begin(), dispatchers_.end(), window) == dispatchers_.end(), null_str);

	if (!dispatchers_.empty()) {
		dispatchers_.back()->set_dirty();
		dispatchers_.back()->window_connect_disconnected(false);

	} else {
		leave();
	} 
}

void thandler::draw()
{
	if (dispatchers_.empty()) {
		return;
	}

	const bool in_scene = event::handler->dispatchers_.front()->is_scene();		
	twindow* window = dispatchers_.back();
	CVideo& video = window->video();

	window->draw();

	// tip's buff will used to other place, require remember first.
	window->draw_float_widgets();
	if (!in_scene) {
		anim2::manager::instance->draw_window_anim();
	}
	cursor::draw();

	video.flip();

	cursor::undraw();
	if (!in_scene) {
		anim2::manager::instance->undraw_window_anim();
	}
	window->undraw_float_widgets();

	if (!window->drawn()) {
		window->set_drawn();
		window->dialog()->app_first_drawn();
	}
}

void thandler::video_resize(const tpoint& new_size)
{
	SDL_Log("thandler::video_resize, new_size: %ix%i\n", new_size.x, new_size.y);

	if (dispatchers_.size() > 1) {
		// when tow or more, forbidden scale.
		// It is bug, require deal with in the feature.

		// I cannot change SDL_VIDEO_RESIZE runtime. replace with recovering window size.
		dispatchers_.front()->video().sdl_set_window_size(settings::screen_width, settings::screen_height);
		return;
	}

	BOOST_FOREACH(twindow* dispatcher, dispatchers_) {
		dispatcher->fire(SDL_VIDEO_RESIZE, *dynamic_cast<twidget*>(dispatcher), new_size, tpoint(0, 0));
	}
}

void thandler::mouse_button_up(const tpoint& position, const Uint8 button)
{
	if (button == SDL_BUTTON_LEFT && twindow::eat_left_up) {
		SDL_Log("thandler::mouse_button_up[#%i], eat_left_up(%i) isn't 0, so decrease.\n", button, twindow::eat_left_up);
		twindow::eat_left_up = false;
		return;
	}
	if (dispatchers_.empty()) {
		return;
	}

	const size_t previous_windows = dispatchers_.size();
	twindow& window = *dispatchers_.back();
	tdistributor::tmouse_button* mouse_button = nullptr;

	switch (button) {
		case SDL_BUTTON_LEFT :
			mouse_button = &window.distributor().mouse_left_;
			break;
		case SDL_BUTTON_MIDDLE :
			mouse_button = &window.distributor().mouse_middle_;
			break;
		case SDL_BUTTON_RIGHT :
			mouse_button = &window.distributor().mouse_right_;
			break;

		default:
			return;
	}

	bool handled = false;
	mouse_button->signal_handler_sdl_button_up(handled, position);
}

void thandler::mouse_button_down(const tpoint& position, const Uint8 button)
{
	if (dispatchers_.empty()) {
		return;
	}

	twindow& window = *dispatchers_.back();
	tdistributor::tmouse_button* mouse_button = nullptr;

	if (window.is_closing()) {
		if (button == SDL_BUTTON_LEFT) {
			// maybe down,up very fast.
			// since this left_down receive by closing window, next left_up requrie discard.
			SDL_Log("mouse_button_down(SDL_BUTTON_LEFT), is_closing, set eat_left_up(%s) to true\n", twindow::eat_left_up? "true:": "false");
			twindow::eat_left_up = true;
		}
		return;
	}

	switch (button) {
		case SDL_BUTTON_LEFT:
			mouse_button = &window.distributor().mouse_left_;
			break;
		case SDL_BUTTON_MIDDLE:
			mouse_button = &window.distributor().mouse_middle_;
			break;
		case SDL_BUTTON_RIGHT:
			mouse_button = &window.distributor().mouse_right_;
			break;
		default:
			return;
	}

	bool handled = false;
	mouse_button->signal_handler_sdl_button_down(handled, position);
	if (!handled) {
		bool halt = false;
		window.signal_handler_click_dismiss2(handled, halt, position);
	}
}

tdispatcher* thandler::keyboard_dispatcher()
{
	for (std::vector<twindow*>::reverse_iterator ritor =
			dispatchers_.rbegin(); ritor != dispatchers_.rend(); ++ritor) {

		if((**ritor).get_want_keyboard_input()) {
			return *ritor;
		}
	}

	return NULL;
}

void thandler::hat_motion(const SDL_JoyHatEvent& event)
{
/*	const hotkey::hotkey_item& hk = hotkey::get_hotkey(event);
	bool done = false;
	if(!hk.null()) {
		done = hotkey_pressed(hk);
	}
	if(!done) {
		//TODO fendrin think about handling hat motions that are not bound to a hotkey.
	} */
}

void thandler::button_down(const SDL_JoyButtonEvent& event)
{
/*	const hotkey::hotkey_item& hk = hotkey::get_hotkey(event);
	bool done = false;
	if(!hk.null()) {
		done = hotkey_pressed(hk);
	}
	if(!done) {
		//TODO fendrin think about handling button down events that are not bound to a hotkey.
	} */
}

void thandler::key_down(const SDL_KeyboardEvent& event)
{
	const hotkey::hotkey_item& hk = hotkey::get_hotkey(event);
	bool done = false;
	// let return go through on android
	if (event.keysym.sym != SDLK_RETURN && !hk.null()) {
		done = hotkey_pressed(hk);
	}
	if(!done) {
		SDL_Keymod mod = (SDL_Keymod)event.keysym.mod;
		key_down(event.keysym.sym, mod, event.keysym.unused);
	}
}

void thandler::text_input(const SDL_TextInputEvent& event)
{
	if (dispatchers_.empty()) {
		return;
	}
	twindow& window = *dispatchers_.back();
	if (window.is_closing()) {
		return;
	}

	window.distributor().signal_handler_sdl_text_input(event.text);
}

bool thandler::hotkey_pressed(const hotkey::hotkey_item& key)
{
	tdispatcher* dispatcher = keyboard_dispatcher();

	if(!dispatcher) {
		return false;
	}

	return dispatcher->execute_hotkey(key.get_id());
}

void thandler::key_down(const SDL_Keycode key
		, const SDL_Keymod modifier
		, const Uint16 unicode)
{
	if (dispatchers_.empty()) {
		return;
	}
	twindow& window = *dispatchers_.back();
	if (window.is_closing()) {
		return;
	}

	window.distributor().signal_handler_sdl_key_down(key, modifier, unicode);
}


/***** tmanager class. *****/

tmanager::tmanager()
{
	handler = new thandler();
}

tmanager::~tmanager()
{
	delete handler;
	handler = NULL;
}

/***** free functions class. *****/

void connect_dispatcher(twindow* dispatcher)
{
	VALIDATE(handler && dispatcher, null_str);
	handler->connect(dispatcher);
}

void disconnect_dispatcher(twindow* dispatcher)
{
	VALIDATE(handler && dispatcher, null_str);
	handler->disconnect(dispatcher);
}

} // namespace event

void absolute_draw()
{
	event::handler->draw();
}

std::vector<twindow*> connectd_window()
{
	return event::handler->dispatchers_;
}

bool is_in_dialog()
{
	if (!event::handler || event::handler->dispatchers_.empty()) {
		return false;
	}
	if (event::handler->dispatchers_.size() == 1) {
		twindow* dispatcher = event::handler->dispatchers_.front();
		if (dispatcher->is_scene()) {
			return false;
		}
	}
	return true;
}

void clear_textures()
{
	if (!event::handler || event::handler->dispatchers_.empty()) {
		return;
	}
	for (std::vector<twindow*>::const_iterator it = event::handler->dispatchers_.begin(); it != event::handler->dispatchers_.end(); ++ it) {
		twindow* window = *it;
		window->clear_texture();
	}
}

} // namespace gui2

