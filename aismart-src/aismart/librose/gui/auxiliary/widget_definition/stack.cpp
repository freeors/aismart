/* $Id: stack.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/auxiliary/widget_definition/stack.hpp"

#include "gettext.hpp"
#include "wml_exception.hpp"

namespace gui2 {

tstack_definition::tstack_definition(const config& cfg)
	: tcontrol_definition(cfg)
{
	load_resolutions<tresolution>(cfg);
}

tstack_definition::tresolution::tresolution(const config& cfg)
	: tresolution_definition_(cfg)
{
/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = 1_stack
 *
 * == Stacked widget ==
 *
 * A stacked widget holds several widgets on top of eachother. This can be used
 * for various effects; add an optional overlay to an image, stack it with a
 * spacer to force a minimum size of a widget. The latter is handy to avoid
 * making a separate definition for a single instance with a fixed size.
 *
 * A stacked widget has no states.
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="stack_definition"}{min=0}{max=-1}{super="generic/widget_definition"}
 * @begin{tag}{name="resolution"}{min=0}{max=-1}{super="generic/widget_definition/resolution"}
 * @allow{link}{name="gui/window/resolution/grid"}
 * @end{tag}{name="resolution"}
 * @end{tag}{name="stack_definition"}
 * @end{parent}{name="gui/"}
 */

	// Add a dummy state since every widget needs a state.
	static config dummy ("draw");
	state.push_back(tstate_definition(dummy));
}

} // namespace gui2

