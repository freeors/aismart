/* $Id: toggle_panel.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/toggle_panel.hpp"

#include "gui/auxiliary/widget_definition/toggle_panel.hpp"
#include "gui/auxiliary/window_builder/toggle_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "sound.hpp"
#include "theme.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_WIDGET(toggle_panel)

ttoggle_panel::ttoggle_panel()
	: tpanel(COUNT)
	, state_(ENABLED)
	, retval_(0)
	, frame_(false)
	, hlt_selected_(true)
	, hlt_focussed_(true)
	, did_state_pre_change_()
	, did_state_changed_()
	, did_double_click_()
{
	set_wants_mouse_left_double_click();

	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_enter, this, _2, _3), event::tdispatcher::back_post_child);
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_leave, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_leave, this, _2, _3), event::tdispatcher::back_post_child);

	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				&ttoggle_panel::signal_handler_left_button_click
					, this, _2, _3, _4));
	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_click
				, this, _2, _3, _4), event::tdispatcher::back_post_child);

	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3, _4));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3, _4)
			, event::tdispatcher::back_post_child);

	connect_signal<event::RIGHT_BUTTON_UP>(boost::bind(
				&ttoggle_panel::signal_handler_right_button_up
					, this, _3, _4, _5));
	connect_signal<event::RIGHT_BUTTON_UP>(boost::bind(
				  &ttoggle_panel::signal_handler_right_button_up
				, this, _3, _4, _5), event::tdispatcher::back_post_child);
}

void ttoggle_panel::set_child_members(const std::map<std::string, std::string>& data)
{
	// typedef boost problem work around.
	tgrid& grid2 = grid();
	for (std::map<std::string, std::string>::const_iterator it = data.begin(); it != data.end(); ++ it) {
		tcontrol* control = dynamic_cast<tcontrol*>(grid2.find(it->first, false));
		if (control) {
			control->set_label(it->second);
		}
	}
}

void ttoggle_panel::set_child_label(const std::string& id, const std::string& label)
{
	tcontrol* control = dynamic_cast<tcontrol*>(grid().find(id, false));
	if (control) {
		control->set_label(label);
	}
}

void ttoggle_panel::set_child_icon(const std::string& id, const std::string& icon)
{
	tcontrol* control = dynamic_cast<tcontrol*>(find(id, false));
	if (control) {
		control->set_icon(icon);
	}
}

twidget* ttoggle_panel::find_at(const tpoint& coordinate, const bool must_be_active)
{
	twidget* result = tcontainer_::find_at(coordinate, must_be_active);
	return result ? result : tcontrol::find_at(coordinate, must_be_active);
}

void ttoggle_panel::set_active(const bool active)
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

bool ttoggle_panel::get_active() const
{
	return state_ != DISABLED && state_ != DISABLED_SELECTED; 
}

tspace4 ttoggle_panel::mini_conf_margin() const
{
	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> confptr =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	const ttoggle_panel_definition::tresolution& conf = *confptr;

	return tspace4{conf.left_margin, conf.right_margin, conf.top_margin, conf.bottom_margin};
}

bool ttoggle_panel::set_value(const bool selected)
{
	if (selected == get_value()) {
		return true;
	}

	if (selected) {
		if (did_state_pre_change_) {
			if (!did_state_pre_change_(*this)) {
				return false;
			}
		}
		set_state(static_cast<tstate>(state_ + ENABLED_SELECTED));
		if (did_state_changed_) {
			did_state_changed_(*this);
		}
	} else {
		set_state(static_cast<tstate>(state_ - ENABLED_SELECTED));
	}
	return true;
}

void ttoggle_panel::set_retval(const int retval)
{
	retval_ = retval;
}

void ttoggle_panel::set_state(const tstate state)
{
	if(state == state_) {
		return;
	}

	state_ = state;
	set_dirty();
}

void ttoggle_panel::update_canvas()
{
	// Inherit.
	tcontrol::update_canvas();

	bool last_row = false;
	tgrid* grid = dynamic_cast<tgrid*>(parent_);
	if (grid && grid->children_vsize()) {
		last_row = grid->child(grid->children_vsize() - 1).widget_ == this;
	}
	
	// set icon in canvases
	uint32_t color;
	BOOST_FOREACH(tcanvas& canva, canvas()) {
		canva.set_variable("last_row", variant(last_row));
		color = hlt_focussed_? theme::instance.item_focus_color: 0;
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		// for mobile device, disable focussed always.
		color = 0;
#endif
		canva.set_variable("focus_color", variant(color));

		color = hlt_selected_? theme::instance.item_highlight_color: 0;
		canva.set_variable("highlight_color", variant(color));
	}
}

bool ttoggle_panel::can_selectable() const
{
	return get_active() && get_visible() == twidget::VISIBLE;
}

void ttoggle_panel::set_canvas_highlight(bool focussed, bool selected) 
{
	bool dirty = false;
	if (hlt_focussed_ != focussed) {
		hlt_focussed_ = focussed;
		dirty = true;
	}
	if (hlt_selected_ != selected) {
		hlt_selected_ = selected;
		dirty = true;
	}
	if (dirty) {
		update_canvas();
	}
}

const std::string& ttoggle_panel::get_control_type() const
{
	static const std::string type = "toggle_panel";
	return type;
}

void ttoggle_panel::signal_handler_mouse_enter(const event::tevent event, bool& handled)
{
	handled = true;
	if (!get_active()) {
		return;
	}
	if (get_value()) {
		set_state(FOCUSSED_SELECTED);
	} else {
		set_state(FOCUSSED);
	}

	if (did_mouse_enter_leave_) {
		did_mouse_enter_leave_(*this, true);
	}
}

void ttoggle_panel::signal_handler_mouse_leave(const event::tevent event, bool& handled)
{
	// don't handled = true, because full window drag require receive MOUSE_LEAVE.
	if (!get_active()) {
		return;
	}
	if (get_value()) {
		set_state(ENABLED_SELECTED);
	} else {
		set_state(ENABLED);
	}

	if (did_mouse_enter_leave_) {
		did_mouse_enter_leave_(*this, false);
	}
}

void ttoggle_panel::signal_handler_left_button_click(const event::tevent event, bool& handled, bool& halt)
{
	handled = true;
	// sound::play_UI_sound(settings::sound_toggle_panel_click);
	if (!set_value(true)) {
		return;
	}

	if (did_click_) {
		did_click_(*this);
	}
}

void ttoggle_panel::signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt)
{
	handled = true;

	if (retval_) {
		twindow* window = get_window();
		window->set_retval(retval_);
	}

	if (did_double_click_) {
		did_double_click_(*this);
	}
}

void ttoggle_panel::signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordiate)
{
	halt = handled = true;
	if (did_right_button_up_) {
		did_right_button_up_(*this);
	}
}

} // namespace gui2

