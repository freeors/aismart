/* $Id: scroll_panel.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_SCROLL_PANEL_HPP_INCLUDED
#define GUI_WIDGETS_SCROLL_PANEL_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_scroll_panel;
}

/**
 * Visible container to hold multiple widgets.
 *
 * This widget can draw items beyond the widgets it holds and in front of
 * them. A panel is always active so these functions return dummy values.
 */
class tscroll_panel: public tscroll_container
{
	friend struct implementation::tbuilder_scroll_panel;
public:

	/**
	 * Constructor.
	 *
	 * @param canvas_count        The canvas count for tcontrol.
	 */
	explicit tscroll_panel(const unsigned canvas_count = 1);

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/** Inherited from tscroll_container. */
	tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) override;

	tpoint calculate_best_size_bh(const int width) override;

private:
	tspace4 mini_conf_margin() const override;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/) {}

};

} // namespace gui2

#endif

