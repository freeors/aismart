/* $Id: stack.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/auxiliary/window_builder/stack.hpp"

#include "config.hpp"
#include "gettext.hpp"

#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/auxiliary/widget_definition/stack.hpp"
#include "gui/widgets/stack.hpp"
#include "wml_exception.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

namespace implementation {

tbuilder_stack::tbuilder_stack(const config& cfg)
	: tbuilder_control(cfg)
	, stack()
	, mode(get_stack_mode(cfg["mode"].str()))
{
	BOOST_FOREACH(const config &layer, cfg.child_range("layer")) {
		stack.push_back(std::make_pair(new tbuilder_grid(layer), layer["grow_factor"].to_unsigned()));
	}
	VALIDATE(!stack.empty(), _("No layer defined."));
}

twidget* tbuilder_stack::build() const
{
	tstack *widget = new tstack();

	init_control(widget);

	boost::intrusive_ptr<const tstack_definition::tresolution> conf =
			boost::dynamic_pointer_cast
				<const tstack_definition::tresolution>(
						widget->config());
	VALIDATE(conf, null_str);

	widget->set_mode(mode);

	// use finalize insteal of init_grid.
	// widget->init_grid(conf->grid);
 
	widget->finalize(stack);

	return widget;
}

} // namespace implementation

} // namespace gui2

/*WIKI
 * @page = GUIToolkitWML
 * @order = 2_stack
 *
 * == Stacked widget ==
 *
 * A stacked widget is a set of widget stacked on top of each other. The
 * widgets are drawn in the layers, in the order defined in the the instance
 * config. By default the last drawn item is also the 'active' layer for the
 * event handling.
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="stack"}{min="0"}{max="-1"}{super="generic/widget_instance"}
 * @begin{table}{config}
 * @end{table}
 * @begin{tag}{name="stack"}{min=0}{max=-1}
 * @begin{tag}{name="layer"}{min=0}{max=-1}{super="gui/window/resolution/grid"}
 * @end{tag}{name="layer"}
 * @end{tag}{name="stack"}
 * @end{tag}{name="stack"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

