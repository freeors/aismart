/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/dialogs/edit_box.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"

#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, edit_box)

tedit_box::tedit_box(tparam& param)
	: param_(param)
{
}

void tedit_box::pre_show()
{
	window_->set_canvas_variable("border", variant("default-border"));

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	if (!param_.remark.empty()) {
		label = find_widget<tlabel>(window_, "remark", false, true);
		label->set_label(param_.remark);
	} else {
		label->set_visible(twidget::INVISIBLE);
	}

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	if (!param_.ok.empty()) {
		ok_->set_label(param_.ok);
	}

	txt_.reset(new ttext_box2(*window_, *find_widget<twidget>(window_, "txt", false, true)));
	window_->keyboard_capture(&txt_->text_box());
	// user_widget->text_box().goto_end_of_data();  now not support, should fixed in future.

	txt_->text_box().set_placeholder(param_.placeholder);
	if (param_.max_length != -1) {
		txt_->text_box().set_maximum_chars(param_.max_length);
	}
	txt_->set_did_text_changed(boost::bind(&tedit_box::text_changed_callback, this, _1));
	txt_->text_box().set_label(param_.def);
}

void tedit_box::post_show()
{
}

void tedit_box::text_changed_callback(ttext_box& widget)
{
	param_.result = widget.label();
	bool active = !param_.result.empty();
	if (active && param_.text_changed) {
		active = param_.text_changed(param_.result);
	}
	ok_->set_active(active);
}

} // namespace gui2

