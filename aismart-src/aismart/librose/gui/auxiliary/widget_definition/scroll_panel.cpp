/* $Id: scroll_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/auxiliary/widget_definition/scroll_panel.hpp"

#include "gettext.hpp"
#include "wml_exception.hpp"

namespace gui2 {

tscroll_panel_definition::tscroll_panel_definition(const config& cfg)
	: tcontrol_definition(cfg)
{
	load_resolutions<tresolution>(cfg);
}

tscroll_panel_definition::tresolution::tresolution(const config& cfg)
	: tresolution_definition_(cfg)
	, grid()
	, top_margin(cfg["top_margin"].to_int())
	, bottom_margin(cfg["bottom_margin"].to_int())
	, left_margin(cfg["left_margin"].to_int())
	, right_margin(cfg["right_margin"].to_int())
{
/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = 1_scrollbar_panel
 *
 * == Scrollbar panel ==
 *
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="scroll_panel_definition"}{min=0}{max=-1}{super="generic/widget_definition"}
 * @begin{tag}{name="resolution"}{min=0}{max=-1}{super="gui/window_definition/resolution"}
 * The definition of a panel with scrollbars. A panel is a container holding
 * other elements in its grid. A panel is always enabled and can't be
 * disabled. Instead it uses the states as layers to draw on.
 *
 * @begin{table}{config}
 *     grid & grid & &                    A grid containing the widgets for main
 *                                     widget. $
 * @end{table}
 * The following layers exist:
 * * background, the background of the panel.
 * * foreground, the foreground of the panel.
 *
 * @end{tag}{name="resolution"}
 * @end{tag}{name="scroll_panel_definition"}
 * @end{parent}{name="gui/"}
 */
	VALIDATE(!(left_margin % 4) && !(right_margin % 4) && !(top_margin % 4) && !(bottom_margin % 4), null_str);
	left_margin = cfg_2_os_size(left_margin);
	right_margin = cfg_2_os_size(right_margin);
	top_margin = cfg_2_os_size(top_margin);
	bottom_margin = cfg_2_os_size(bottom_margin);

	// The panel needs to know the order.
	state.push_back(tstate_definition(cfg.child("background")));

	const config &child = cfg.child("grid");
	VALIDATE(child, _("No grid defined."));

	grid = new tbuilder_grid(child);
}

} // namespace gui2

