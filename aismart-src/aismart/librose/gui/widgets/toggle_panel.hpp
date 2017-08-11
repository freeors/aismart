/* $Id: toggle_panel.hpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#ifndef GUI_WIDGETS_TOGGLE_PANEL_HPP_INCLUDED
#define GUI_WIDGETS_TOGGLE_PANEL_HPP_INCLUDED

#include "gui/widgets/panel.hpp"

namespace gui2 {

class tlistbox;

/**
 * Class for a toggle button.
 *
 * Quite some code looks like ttoggle_button maybe we should inherit from that but let's test first.
 * the problem is that the toggle_button has an icon we don't want, but maybe look at refactoring later.
 * but maybe we should also ditch the icon, not sure however since it's handy for checkboxes...
 */
class ttoggle_panel : public tpanel
{
public:
	ttoggle_panel();

	void set_child_members(const std::map<std::string, std::string>& data);
	void set_child_icon(const std::string& id, const std::string& icon);

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontainer_ */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active);

	/** Inherited from tpanel. */
	void set_active(const bool active);

	/** Inherited from tpanel. */
	bool get_active() const;

	/** Inherited from tpanel. */
	unsigned get_state() const { return state_; }

	/**
	 * Inherited from tpanel.
	 *
	 * @todo only due to the fact our definition is slightly different from
	 * tpanel_definition we need to override this function and do about the same,
	 * look at a way to 'fix' that.
	 */

	/** Inherited from tselectable_ */
	bool get_value() const { return state_ >= ENABLED_SELECTED; }

	/** Inherited from tselectable_ */
	bool set_value(const bool selected);

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_retval(const int retval);

	/** Inherited from tselectable_. */
	void set_did_mouse_enter_leave(const boost::function<void (ttoggle_panel&, const bool)>& did)
	{
		did_mouse_enter_leave_ = did; 
	}
	void set_did_state_pre_change(const boost::function<bool (ttoggle_panel&)>& did)
	{
		did_state_pre_change_ = did; 
	}
	void set_did_state_changed(const boost::function<void (ttoggle_panel&)>& did)
	{ 
		did_state_changed_ = did; 
	}
	void set_did_click(const boost::function<void(ttoggle_panel&)>& did)
	{
		did_click_ = did;
	}
	void set_did_double_click(const boost::function<void (ttoggle_panel&)>& did)
	{ 
		did_double_click_ = did; 
	}
	void set_did_right_button_up(const boost::function<void (ttoggle_panel&)>& did)
	{ 
		did_right_button_up_ = did; 
	}

	void set_data(unsigned int data) { data_ = data; }
	unsigned int get_data() const { return data_; }

	void set_canvas_highlight(bool focussed, bool selected);

	bool can_selectable() const;

	/** Inherited from tcontrol. */
	void update_canvas();

protected:
	void set_child_label(const std::string& id, const std::string& label);
	tspace4 mini_conf_margin() const override;

private:
	/** Inherited from tpanel. */
	void impl_draw_background(texture& frame_buffer, int x_offset, int y_offset) override
	{
		// We don't have a fore and background and need to draw depending on
		// our state, like a control. So we use the controls drawing method.
		tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);
	}

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

private:
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

	/**
	 * The toggle panel can contain an private data.
	 */
	unsigned int data_;

	bool hlt_selected_;
	bool hlt_focussed_;
	bool frame_;

	boost::function<void (ttoggle_panel&, const bool)> did_mouse_enter_leave_;

	/** See tselectable_::set_did_state_change. */
	boost::function<bool (ttoggle_panel&)> did_state_pre_change_;

	/** See tselectable_::set_did_state_changed. */
	boost::function<void (ttoggle_panel&)> did_state_changed_;

	boost::function<void(ttoggle_panel&)> did_click_;

	/** Mouse left double click callback */
	boost::function<void (ttoggle_panel&)> did_double_click_;

	boost::function<void (ttoggle_panel&)> did_right_button_up_;

	/** Inherited from tpanel. */
	const std::string& get_control_type() const;

protected:
	/***** ***** ***** signal handlers ***** ****** *****/

	virtual void signal_handler_mouse_enter(const event::tevent event, bool& handled);
	virtual void signal_handler_mouse_leave(const event::tevent event, bool& handled);

	virtual void signal_handler_left_button_click(const event::tevent event, bool& handled, bool& halt);
	virtual void signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt);
	virtual void signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordiate);
};

} // namespace gui2

#endif



