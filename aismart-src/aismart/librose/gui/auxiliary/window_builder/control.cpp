/* $Id: control.cpp 54322 2012-05-28 08:21:28Z mordante $ */
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

#include "gui/auxiliary/window_builder/control.hpp"

#include "config.hpp"
#include "formatter.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/widgets/control.hpp"
#include "gui/widgets/settings.hpp"
#include "wml_exception.hpp"

namespace gui2 {

namespace implementation {

tbuilder_control::tbuilder_control(const config& cfg)
	: tbuilder_widget(cfg)
	, definition(cfg["definition"])
	, label(cfg["label"].t_str())
	, tooltip(cfg["tooltip"].t_str())
	, width(cfg["width"])
	, height(cfg["height"])
	, width_is_max(cfg["width_is_max"].to_bool())
	, height_is_max(cfg["height_is_max"].to_bool())
	, min_text_width(cfg["min_text_width"])
	, text_font_size(cfg["text_font_size"].to_int())
	, text_color_tpl(cfg["text_color_tpl"].to_int())
{
	if (definition.empty()) {
		definition = "default";
	}
}

void tbuilder_control::init_control(tcontrol* control) const
{
	VALIDATE(control, null_str);

	control->set_id(id);
	control->set_definition(definition, min_text_width);
	control->set_linked_group(linked_group);
	control->set_label(label);
	control->set_tooltip(tooltip);
	if (text_font_size) {
		control->set_text_font_size(text_font_size);
	}
	if (text_color_tpl) {
		control->set_text_color_tpl(text_color_tpl);
	}
	control->set_best_size_2th(width, width_is_max, height, height_is_max);
}

} // namespace implementation

} // namespace gui2

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 1_widget
 *
 * = Widget =
 * @begin{parent}{name="generic/"}
 * @begin{tag}{name="widget_instance"}{min="0"}{max="-1"}
 * All widgets placed in the cell have some values in common:
 * @begin{table}{config}
 *     id & string & "" &              This value is used for the engine to
 *                                     identify 'special' items. This means that
 *                                     for example a text_box can get the proper
 *                                     initial value. This value should be
 *                                     unique or empty. Those special values are
 *                                     documented at the window definition that
 *                                     uses them. NOTE items starting with an
 *                                     underscore are used for composed widgets
 *                                     and these should be unique per composed
 *                                     widget. $
 *
 *     definition & string & "default" &
 *                                     The id of the widget definition to use.
 *                                     This way it's possible to select a
 *                                     specific version of the widget e.g. a
 *                                     title label when the label is used as
 *                                     title. $
 *
 *     linked_group & string & "" &    The linked group the control belongs
 *                                     to. $
 *
 *     label & t_string & "" &          Most widgets have some text associated
 *                                     with them, this field contain the value
 *                                     of that text. Some widgets use this value
 *                                     for other purposes, this is documented
 *                                     at the widget. E.g. an image uses the
 *                                     filename in this field. $
 *
 *     tooltip & t_string & "" &        If you hover over a widget a while (the
 *                                     time it takes can differ per widget) a
 *                                     short help can show up.This defines the
 *                                     text of that message. This field may not
 *                                     be empty when 'help' is set. $
 *
 *
 *     help & t_string & "" &           If you hover over a widget and press F10
 *                                     (or the key the user defined for the help
 *                                     tip) a help message can show up. This
 *                                     help message might be the same as the
 *                                     tooltip but in general (if used) this
 *                                     message should show more help. This
 *                                     defines the text of that message. $
 *
 *    use_tooltip_on_label_overflow & bool & true &
 *                                     If the text on the label is truncated and
 *                                     the tooltip is empty the label can be
 *                                     used for the tooltip. If this variable is
 *                                     set to true this will happen. $
 *
 *
 *   size_text & t_string & "" &       Sets the minimum width of the widget
 *                                     depending on the text in it. (Note not
 *                                     implemented yet.) $
 * @end{table}
 * @end{tag}{name="widget_instance"}
 * @end{parent}{name="generic/"}
 */

