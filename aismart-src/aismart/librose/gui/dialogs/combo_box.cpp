/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "gui/dialogs/combo_box.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/label.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, combo_box)

tcombo_box::tcombo_box(const std::vector<std::string>& items, int initial_sel, const std::string& title, bool allow_cancel)
	: initial_sel_(initial_sel)
	, cursel_(initial_sel)
	, items_(items)
	, title_(title)
	, allow_cancel_(allow_cancel)
{
	VALIDATE(!items.empty(), null_str);
	VALIDATE(initial_sel_ == nposm || initial_sel_ >= 0 && initial_sel_ < (int)items_.size(), null_str);
}

void tcombo_box::pre_show()
{
	window_->set_canvas_variable("border", variant("default-border"));

	tlabel* title = find_widget<tlabel>(window_, "title", false, true);
	if (!title_.empty()) {
		title->set_label(title_);
	} else {
		title->set_visible(twidget::INVISIBLE);
	}
	if (!allow_cancel_) {
		find_widget<twidget>(window_, "ok", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<twidget>(window_, "cancel", false, true)->set_visible(twidget::INVISIBLE);
	}
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	window_->keyboard_capture(&list);

	std::map<std::string, std::string> data;

	for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const std::string& item = *it;
		
		data["label"] = item;
		list.insert_row(data);
	}

	if (cursel_ != nposm) {
		list.select_row(cursel_);
	}
	list.set_did_row_changed(boost::bind(&tcombo_box::item_selected, this, boost::ref(*window_), _1, _2));
}

void tcombo_box::item_selected(twindow& window, tlistbox& list, ttoggle_panel& widget)
{
	if (!allow_cancel_) {
		window.set_retval(twindow::OK);
	}
}

void tcombo_box::post_show()
{
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	ttoggle_panel* selected = list.cursel();
	cursel_ = selected? selected->at(): nposm;
}

}
