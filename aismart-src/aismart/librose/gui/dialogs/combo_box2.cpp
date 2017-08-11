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

#include "gui/dialogs/combo_box2.hpp"

#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, combo_box2)

tcombo_box2::tcombo_box2(const std::string& title, const std::vector<std::string>& items, int initial_sel)
	: title_(title)
	, initial_sel_(initial_sel)
	, cursel_(initial_sel != nposm? initial_sel: 0)
	, items_(items)
{
	VALIDATE(!items.empty(), null_str);
	VALIDATE(initial_sel == nposm || initial_sel >= 0 && initial_sel < (int)items_.size(), null_str);
}

void tcombo_box2::pre_show()
{
	// window.set_transition_fade(ttransition::fade_left);
	window_->set_label("misc/white-background.png");

	tlabel* title = find_widget<tlabel>(window_, "title", false, true);
	title->set_label(title_);

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	ok_->set_visible(twidget::HIDDEN);

	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	window_->keyboard_capture(&list);

	std::map<std::string, std::string> data;

	int at = 0;
	for (std::vector<std::string>::const_iterator it = items_.begin(); it != items_.end(); ++ it) {
		const std::string& item = *it;
		
		data["label"] = item;
		list.insert_row(data);

		at ++;
	}
	list.set_did_row_changed(boost::bind(&tcombo_box2::item_selected, this, boost::ref(*window_), _1));
	list.select_row(cursel_);
}

void tcombo_box2::item_selected(twindow& window, tlistbox& list)
{
	ttoggle_panel* selected = list.cursel();
	cursel_ = selected? selected->at(): nposm;

	ok_->set_visible(cursel_ != initial_sel_? twidget::VISIBLE: twidget::HIDDEN);

	if (did_item_changed_) {
		did_item_changed_(list, cursel_);
	}
}

void tcombo_box2::post_show()
{
	tlistbox& list = find_widget<tlistbox>(window_, "listbox", false);
	ttoggle_panel* selected = list.cursel();
	VALIDATE(selected, null_str);
	cursel_ = selected->at();
}

}
