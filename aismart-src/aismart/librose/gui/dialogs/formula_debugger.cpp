/* $Id: formula_debugger.cpp 48870 2011-03-13 07:49:16Z mordante $ */
/*
   Copyright (C) 2009 - 2011 by Yurii Chernyi <terraninfo@terraninfo.net>
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

#include "gui/dialogs/formula_debugger.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "../../formula_debugger.hpp"
#include "font.hpp"

#include <boost/foreach.hpp>
#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_formula_debugger
 *
 * == Formula debugger ==
 *
 * This shows the debugger for the formulas.
 *
 * @begin{table}{dialog_widgets}
 *
 * stack & & control & m &
 *         A stack. $
 *
 * execution & & control & m &
 *         Execution trace label. $
 *
 * state & & control & m &
 *         The state. $
 *
 * step & & button & m &
 *         Button to step into the execution. $
 *
 * stepout & & button & m &
 *         Button to step out of the execution. $
 *
 * next & & button & m &
 *         Button to execute the next statement. $
 *
 * continue & & button & m &
 *         Button to continue the execution. $
 *
 * @end{table}
 */

REGISTER_DIALOG(rose, formula_debugger)

void tformula_debugger::pre_show()
{
	window_->set_canvas_variable("border", variant("default-border"));
	// stack label
	tcontrol* stack_label = find_widget<tcontrol>(
			window_, "stack", false, true);

	std::stringstream stack_text;
	std::string indent = "  ";
	int c = 0;
	BOOST_FOREACH (const game_logic::debug_info &i, fdb_.get_call_stack()) {
		for(int d = 0; d < c; ++d) {
			stack_text << indent;
		}
		stack_text << "#" << ht::generate_format(i.counter(), color_to_uint32(font::GOOD_COLOR))
				<<": \""<< ht::generate_format(i.name(), color_to_uint32(font::GOOD_COLOR))
				<< "\": '" << i.str() << "' " << std::endl;
		++c;
	}

	stack_label->set_label(stack_text.str());
	window_->keyboard_capture(stack_label);

	// execution trace label
	tcontrol* execution_label = find_widget<tcontrol>(
			window_, "execution", false, true);

	std::stringstream execution_text;
	BOOST_FOREACH (const game_logic::debug_info &i, fdb_.get_execution_trace()) {
		for(int d = 0; d < i.level(); ++d) {
			execution_text << indent;
		}
		if(!i.evaluated()) {
			execution_text << "#" << ht::generate_format(i.counter(), color_to_uint32(font::GOOD_COLOR))
					<< ": \"" << ht::generate_format(i.name(), color_to_uint32(font::GOOD_COLOR))
					<< "\": '" << i.str() << "' " << std::endl;
		} else {
			execution_text << "#" << ht::generate_format(i.counter(), color_to_uint32(font::BLUE_COLOR))
					<< ": \"" << ht::generate_format(i.name(), color_to_uint32(font::BLUE_COLOR))
					<< "\": '" << i.str() << "' = "
					<< ht::generate_format(i.value().to_debug_string(NULL,false), color_to_uint32(font::BAD_COLOR))
					<< std::endl;
		}
	}

	execution_label->set_label(execution_text.str());

	// state
	std::string state_str;
	bool is_end = false;
	if(!fdb_.get_current_breakpoint()) {
		state_str = "";
	} else {
		state_str = fdb_.get_current_breakpoint()->name();
	        if(state_str=="End") {
			is_end = true;
		}
	}

	find_widget<tcontrol>(window_, "state", false).set_label(state_str);

	// callbacks
	tbutton& step_button = find_widget<tbutton>(window_, "step", false);
	connect_signal_mouse_left_click(step_button, boost::bind(
			  &tformula_debugger::callback_step_button
			, this
			, boost::ref(*window_)));

	tbutton& stepout_button = find_widget<tbutton>(window_, "stepout", false);
	connect_signal_mouse_left_click(stepout_button, boost::bind(
			  &tformula_debugger::callback_stepout_button
			, this
			, boost::ref(*window_)));

	tbutton& next_button = find_widget<tbutton>(window_, "next", false);
	connect_signal_mouse_left_click(next_button, boost::bind(
			  &tformula_debugger::callback_next_button
			, this
			, boost::ref(*window_)));

	tbutton& continue_button = find_widget<tbutton>(window_, "continue", false);
	connect_signal_mouse_left_click(continue_button, boost::bind(
			  &tformula_debugger::callback_continue_button
			, this
			, boost::ref(*window_)));

	if(is_end) {
		step_button.set_active(false);
		stepout_button.set_active(false);
		next_button.set_active(false);
		continue_button.set_active(false);
	}
}

void tformula_debugger::callback_continue_button(twindow& window)
{
	fdb_.add_breakpoint_continue_to_end();
	window.set_retval(twindow::OK);
}

void tformula_debugger::callback_next_button(twindow& window)
{
	fdb_.add_breakpoint_next();
	window.set_retval(twindow::OK);
}

void tformula_debugger::callback_step_button(twindow& window)
{
	fdb_.add_breakpoint_step_into();
	window.set_retval(twindow::OK);
}

void tformula_debugger::callback_stepout_button(twindow& window)
{
	fdb_.add_breakpoint_step_out();
	window.set_retval(twindow::OK);
}

} //end of namespace gui2
