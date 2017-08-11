/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/erase.hpp"

#include "game_config.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/text_box2.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include <time.h>
#include "wml_exception.hpp"
#include "language.hpp"
#include "net.hpp"
#include "hero.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(aismart, erase)

terase::terase()
	: erase_(nullptr)
	, next_sms_ticks_(0)
{
}

void terase::pre_show()
{
	window_->set_label("misc/white-background.png");

	vericode_timer_.reset(400, *window_, boost::bind(&terase::vericode_timer_handler, this));

	std::stringstream ss;
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);

	tbutton* button = find_widget<tbutton>(window_, "cancel", false, true);
	button->set_icon("misc/back.png");

	get_vericode_ = find_widget<tbutton>(window_, "get_vericode", false, true);
	get_vericode_->set_label(_("Get"));
	get_vericode_->set_canvas_variable("border", variant("login2-border"));
	connect_signal_mouse_left_click(
		*get_vericode_
		, boost::bind(
			&terase::click_get_vericode
			, this));

	erase_ = find_widget<tbutton>(window_, "erase", false, true);
	erase_->set_canvas_variable("border", variant("login2-border"));
	erase_->set_active(false);
	connect_signal_mouse_left_click(
		*erase_
		, boost::bind(
			&terase::click_erase
			, this));

	// verification code
	vericode_.reset(new ttext_box2(*window_, *find_widget<twidget>(window_, "vericode", false, true), null_str, "misc/email.png", false, null_str, false));
	vericode_->set_did_text_changed(boost::bind(&terase::did_field_changed, this));
	vericode_->text_box().set_maximum_chars(max_vericode_size);
	ss.str("");
	ss << _("Verification code");
	vericode_->text_box().set_placeholder(ss.str());

	label = find_widget<tlabel>(window_, "warning", false, true);
	label->set_label(ht::generate_format(_("Once written off, all your data on the server will be deleted, including mobile number, password, nickname, models, test scripts, image files, comments, and so on."), 0xffff0000));
}

void terase::post_show()
{
	vericode_timer_.reset();
}

void terase::set_erase_active(const std::string& vericode)
{
	bool active = check_vericode_size(vericode.size());
	erase_->set_active(active);
}

void terase::set_vericode_active()
{
	get_vericode_->set_active(!next_sms_ticks_);
}

void terase::did_field_changed()
{
	set_erase_active(vericode_->text_box().label());
}

void terase::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

void terase::click_get_vericode()
{
	VALIDATE(current_user.valid(), null_str);
	bool ret = run_with_progress(null_str, boost::bind(&net::do_vericode, current_user.mobile, vericode_eraseuser), false, 2000, false);
	if (ret) {
		get_vericode_->set_active(false);
		next_sms_ticks_ = SDL_GetTicks() + vericode_reserve_secs * 1000;
	}
}

void terase::click_erase()
{
	bool ret = run_with_progress(null_str, boost::bind(&net::do_erase_user, current_user.sessionid, vericode_->text_box().label()), false, 2000, false);
	if (ret) {
		// switch to sesame account
		current_user.sessionid.clear();
		preferences::set_startup_login(false);

		window_->set_retval(twindow::OK);
	}
}

void terase::vericode_timer_handler()
{
	if (next_sms_ticks_) {
		Uint32 now = SDL_GetTicks();
		std::stringstream ss;

		if (now < next_sms_ticks_) {
			int remainder = (next_sms_ticks_ - now) / 1000 + 1;
			ss << remainder << dsgettext("rose-lib", "time^s");
		} else {
			next_sms_ticks_ = 0;
			ss << _("Get");
		}
		get_vericode_->set_label(ss.str());
		set_vericode_active();
	}
}

} // namespace gui2

