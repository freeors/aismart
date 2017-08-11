/* $Id: toggle_button.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TOGGLE_BUTTON_HPP_INCLUDED
#define GUI_WIDGETS_TOGGLE_BUTTON_HPP_INCLUDED

#include "gui/widgets/control.hpp"

namespace gui2 {

/**
 * Class for a toggle button.
 *
 * A toggle button is a button with two states 'up' and 'down' or 'selected' and
 * 'deselected'. When the mouse is pressed on it the state changes.
 */
class ttoggle_button : public tcontrol
{
public:
	ttoggle_button();

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool active);

	/** Inherited from tcontrol. */
	bool get_active() const
		{ return state_ != DISABLED && state_ != DISABLED_SELECTED; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return state_; }

	/** Inherited from tcontrol. */
	void update_canvas();

	/** Inherited from tselectable_ */
	bool get_value() const { return state_ >= ENABLED_SELECTED; }

	/** Inherited from tselectable_ */
	bool set_value(const bool selected);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_retval(const int retval);

	/** Inherited from tselectable_. */
	void set_did_state_pre_change(const boost::function<bool (ttoggle_button&)>& callback)
		{ did_state_pre_change_ = callback; }

	/** Inherited from tselectable_. */
	void set_did_state_changed(const boost::function<void (ttoggle_button&)>& callback)
		{ did_state_changed_ = callback; }

	void set_data(unsigned int data) { data_ = data; }
	unsigned int get_data() const { return data_; }

	void set_radio(bool val) { radio_ = val; }
	bool radio() const { return radio_; }

	bool can_selectable() const;

private:
	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 * Also note the internals do assume the order for 'up' and 'down' to be the
	 * same and also that 'up' is before 'down'. 'up' has no suffix, 'down' has
	 * the SELECTED suffix.
	 */
	enum tstate {
		ENABLED,          DISABLED,          FOCUSSED,
		ENABLED_SELECTED, DISABLED_SELECTED, FOCUSSED_SELECTED,
		COUNT};

	void set_state(const tstate state);

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/**
	 * The return value of the button.
	 *
	 * If this value is not 0 and the button is double clicked it sets the
	 * retval of the window and the window closes itself.
	 */
	int retval_;

	/** See tselectable_::set_callback_state_changed. */
	boost::function<bool (ttoggle_button&)> did_state_pre_change_;

	/** See tselectable_::set_did_state_changed. */
	boost::function<void (ttoggle_button&)> did_state_changed_;

	/**
	 * The toggle button can contain an private data.
	 */
	unsigned int data_;
	bool radio_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_mouse_enter(const event::tevent event, bool& handled);

	void signal_handler_mouse_leave(const event::tevent event, bool& handled);

	void signal_handler_left_button_click(const event::tevent event, bool& handled);

	void signal_handler_left_button_double_click(const event::tevent event, bool& handled);
};

ttoggle_button* create_toggle_button(const std::string& id, const std::string& definition, const std::string& label);

} // namespace gui2

#endif

