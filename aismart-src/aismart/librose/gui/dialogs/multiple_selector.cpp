/* $Id: simple_item_selector.cpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#include "gui/dialogs/multiple_selector.hpp"

#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, multiple_selector)

tmultiple_selector::tmultiple_selector(const std::string& title, const std::vector<std::string>& items, const std::set<int>& initial_selected)
	: title_(title)
	, items_(items)
	, initial_selected_(initial_selected)
	, ok_(nullptr)
{
}

void tmultiple_selector::pre_show()
{
	window_->set_label("misc/white-background.png");

	tlabel& title = find_widget<tlabel>(window_, "title", false);
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	ok_->set_visible(twidget::HIDDEN);

	title.set_label(title_);
	states_.resize(items_.size(), false);

	std::map<std::string, std::string> data;
	for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		data["item"] = *it;

		ttoggle_panel& row = list.insert_row(data);
		ttoggle_button* widget = dynamic_cast<ttoggle_button*>(row.find("select", false));
		widget->set_did_state_changed(boost::bind(&tmultiple_selector::did_selector_changed, this, _1, std::distance(items_.begin(), it)));
		if (initial_selected_.find(row.at()) != initial_selected_.end()) {
			widget->set_value(true);
		}
	}
}

void tmultiple_selector::post_show()
{
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	int rows = list.rows();

	for (int at = 0; at < rows; at ++) {
		ttoggle_panel& row = list.row_panel(at);
		ttoggle_button* widget = dynamic_cast<ttoggle_button*>(row.find("select", false));
		if (widget->get_value()) {
			selected_.insert(at);
		}
	}
}

void tmultiple_selector::did_selector_changed(ttoggle_button& widget, int at)
{
	states_[at] = widget.get_value();
	bool ok_visible = true;
	if (did_selector_changed_) {
		ok_visible = did_selector_changed_(widget, at, boost::ref(states_));
	}
	ok_->set_visible(ok_visible? twidget::VISIBLE: twidget::HIDDEN);
}

}
