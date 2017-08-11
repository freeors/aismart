/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/text_box2.hpp"

#include "gui/widgets/settings.hpp"
#include "gui/widgets/panel.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

ttext_box2::ttext_box2(twindow& window, twidget& widget, const std::string& panel_border, const std::string& image_label, bool password, const std::string& button_label, bool clear)
	: tbase_tpl_widget(window, widget)
	, widget_(dynamic_cast<tpanel*>(&widget))
	, image_(find_widget<timage>(&widget, "rose__text_box2_image", false, true))
	, text_box_(find_widget<ttext_box>(&widget, "rose__text_box2_text_box", false, true))
	, button_(find_widget<tbutton>(&widget, "rose__text_box2_button", false, true))
	, clear_(clear)
{
	widget_->set_border(panel_border);
	widget_->set_margin(0, 0, 0, 0);

	if (image_label.empty()) {
		image_->set_visible(twidget::INVISIBLE);
	} else {
		image_->set_label(image_label);
	}

	text_box_->set_border(null_str);
	if (password) {
		text_box_->set_password();
	}
	text_box_->set_did_text_changed(boost::bind(&ttext_box2::did_text_changed, this, _1));

	button_->set_label(button_label);
	button_->set_visible(twidget::HIDDEN);
	if (clear_) {
		connect_signal_mouse_left_click(
			*button_
			, boost::bind(
				&ttext_box2::clear_text_box
				, this));
	}

}

void ttext_box2::did_text_changed(ttext_box& widget)
{
	const std::string& label = widget.label();
	button_->set_visible(label.empty()? twidget::HIDDEN: twidget::VISIBLE);

	if (did_text_changed_) {
		did_text_changed_(*text_box_);
	}
}

void ttext_box2::clear_text_box()
{
	text_box_->set_label(null_str);
}

} // namespace gui2

