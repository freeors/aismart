/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_SCROLL_TEXT_BOX_HPP_INCLUDED
#define GUI_WIDGETS_SCROLL_TEXT_BOX_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"
#include "gui/widgets/text_box.hpp"
namespace gui2 {

class tspacer;

namespace implementation {
	struct tbuilder_scroll_text_box;
}

/**
 * Label showing a text.
 *
 * This version shows a scrollbar if the text gets too long and has some
 * scrolling features. In general this widget is slower as the normal label so
 * the normal label should be preferred.
 */
class tscroll_text_box : public tscroll_container
{
	friend struct implementation::tbuilder_scroll_text_box;
public:

	tscroll_text_box();

	ttext_box* tb() const { return tb_; }

	/** Inherited from tcontrol. */
	void set_text_editable(bool editable) override;

	/** Inherited from tcontainer_. */
	void set_self_active(const bool active)
		{ state_ = active ? ENABLED : DISABLED; }

	/** Inherited from ttext_box. */
	const std::string& label() const;
	void set_label(const std::string& text);
	void set_border(const std::string& border);

	void set_atom_markup(const bool atom_markup);
	void set_placeholder(const std::string& label);
	void set_default_color(const uint32_t color);
	void set_maximum_chars(const size_t maximum_chars);
	void set_did_text_changed(const boost::function<void (ttext_box& textboxt) >& did)
	{
		did_text_changed_ = did;
	}

	void set_did_extra_menu(const boost::function<void (ttext_box& widget, std::vector<tfloat_widget*>&)>& did)
	{
		tb_->set_did_extra_menu(did);
	}
	void insert_str(const std::string& str);
	void insert_img(const std::string& str);

	/** Inherited from tscroll_container. */

	/***** ***** ***** setters / getters for members ***** ****** *****/

	bool get_active() const { return state_ != DISABLED; }

	unsigned get_state() const { return state_; }

private:
	void mini_finalize_subclass() override;
	bool mini_mouse_motion(const tpoint& first, const tpoint& last) override;
	tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) override;

	void did_text_changed(ttext_box& widget);
	void did_cursor_moved(ttext_box& widget);

private:
	ttext_box* tb_;
	boost::function<void (ttext_box&)> did_text_changed_;

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

//  It's not needed for now so keep it disabled, no definition exists yet.
//	void set_state(const tstate state);

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/***** ***** ***** inherited ****** *****/

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_left_button_down(const event::tevent event);
};

} // namespace gui2

#endif

