/* $Id: toggle_button.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/toggle_button.hpp"

#include "gui/auxiliary/widget_definition/toggle_button.hpp"
#include "gui/auxiliary/window_builder/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "sound.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

static ttoggle_button* create_toggle_button(const config& cfg)
{
	implementation::tbuilder_toggle_button builder(cfg);
	return dynamic_cast<ttoggle_button*>(builder.build());
}

ttoggle_button* create_toggle_button(const std::string& id, const std::string& definition, const std::string& label)
{
	config cfg;
	if (!id.empty()) {
		cfg["id"] = id;
	}
	if (!label.empty()) {
		cfg["label"] = label;
	}
	cfg["definition"] = definition;
	return create_toggle_button(cfg);
}

REGISTER_WIDGET(toggle_button)

ttoggle_button::ttoggle_button()
	: tcontrol(COUNT)
	, state_(ENABLED)
	, radio_(false)
	, retval_(0)
	, did_state_pre_change_()
	, did_state_changed_()
{
	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_button::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_button::signal_handler_mouse_leave, this, _2, _3));

	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				&ttoggle_button::signal_handler_left_button_click
					, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				&ttoggle_button::signal_handler_left_button_double_click
					, this, _2, _3));
}

void ttoggle_button::set_active(const bool active)
{
	if (active) {
		if (get_value()) {
			set_state(ENABLED_SELECTED);
		} else {
			set_state(ENABLED);
		}
	} else {
		if (get_value()) {
			set_state(DISABLED_SELECTED);
		} else {
			set_state(DISABLED);
		}
	}
}

void ttoggle_button::update_canvas()
{
	// Inherit.
	tcontrol::update_canvas();

	set_dirty();
}

bool ttoggle_button::set_value(const bool selected)
{
	if (selected == get_value()) {
		return true;
	}

	if (selected && did_state_pre_change_) {
		if (!did_state_pre_change_(*this)) {
			return false;
		}
	}

	if (selected) {
		set_state(static_cast<tstate>(state_ + ENABLED_SELECTED));
	} else {
		set_state(static_cast<tstate>(state_ - ENABLED_SELECTED));
	}

	if (did_state_changed_) {
		did_state_changed_(*this);
	}

	return true;
}

void ttoggle_button::set_retval(const int retval)
{
	if (retval == retval_) {
		return;
	}

	retval_ = retval;
	set_wants_mouse_left_double_click(retval_ != 0);
}

void ttoggle_button::set_state(const tstate state)
{
	if (state != state_) {
		state_ = state;
		set_dirty();
	}
}

bool ttoggle_button::can_selectable() const
{
	return get_active() && get_visible() == twidget::VISIBLE;
}

const std::string& ttoggle_button::get_control_type() const
{
	static const std::string type = "toggle_button";
	return type;
}

void ttoggle_button::signal_handler_mouse_enter(
		const event::tevent event, bool& handled)
{
	if (!get_active()) {
		return;
	}

	if(get_value()) {
		set_state(FOCUSSED_SELECTED);
	} else {
		set_state(FOCUSSED);
	}
}

void ttoggle_button::signal_handler_mouse_leave(
		const event::tevent event, bool& handled)
{
	if (!get_active()) {
		return;
	}

	if(get_value()) {
		set_state(ENABLED_SELECTED);
	} else {
		set_state(ENABLED);
	}
}

void ttoggle_button::signal_handler_left_button_click(const event::tevent event, bool& handled)
{
	handled = true;
	if (radio_ && get_value()) {
		return;
	}

	// sound::play_UI_sound(settings::sound_toggle_button_click);
	if (get_value()) {
		set_value(false);
	} else {
		set_value(true);
	}

}

void ttoggle_button::signal_handler_left_button_double_click(const event::tevent event, bool& handled)
{
	handled = true;
	if (retval_ == 0) {
		return;
	}

	twindow* window = get_window();
	window->set_retval(retval_);
}
} // namespace gui2

