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

#include "gui/dialogs/resetpwd.hpp"

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

REGISTER_DIALOG(aismart, resetpwd)

tresetpwd::tresetpwd(bool bind)
	: bind_(bind)
	, register_(NULL)
	, next_sms_ticks_(0)
{
}

void tresetpwd::pre_show()
{
	window_->set_label("misc/white-background.png");

	vericode_timer_.reset(400, *window_, boost::bind(&tresetpwd::vericode_timer_handler, this));

	std::stringstream ss;
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (bind_) {
		ss.str("");
		ss << _("Register") << "(" << _("Bind Sesame Account") << ")";
		label->set_label(ss.str());
	}

	tbutton* button = find_widget<tbutton>(window_, "cancel", false, true);
	button->set_icon("misc/back.png");

	get_vericode_ = find_widget<tbutton>(window_, "get_vericode", false, true);
	get_vericode_->set_label(_("Get"));
	get_vericode_->set_canvas_variable("border", variant("login2-border"));
	get_vericode_->set_active(false);
	connect_signal_mouse_left_click(
		*get_vericode_
		, boost::bind(
			&tresetpwd::click_get_vericode
			, this));

	register_ = find_widget<tbutton>(window_, "reset", false, true);
	register_->set_canvas_variable("border", variant("login2-border"));
	register_->set_active(false);
	connect_signal_mouse_left_click(
		*register_
		, boost::bind(
			&tresetpwd::click_reset
			, this));

	button = find_widget<tbutton>(window_, "agreement", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tresetpwd::user_agreement
			, this
			, boost::ref(*window_)));

	utils::string_map symbols;
	symbols["agreement"] = "Sesame User Agreement";
	std::string agreement = vgettext2("I have read and agreed to $agreement", symbols);
	button->set_label(agreement);

	// mobile
	mobile_.reset(new ttext_box2(*window_, *find_widget<twidget>(window_, "mobile", false, true), null_str, "misc/mobile.png"));
	mobile_->set_did_text_changed(boost::bind(&tresetpwd::did_field_changed, this));
	mobile_->text_box().set_maximum_chars(22);
	mobile_->text_box().set_placeholder(_("Mobile number"));

	// verification code
	vericode_.reset(new ttext_box2(*window_, *find_widget<twidget>(window_, "vericode", false, true), null_str, "misc/email.png", false, null_str, false));
	vericode_->set_did_text_changed(boost::bind(&tresetpwd::did_field_changed, this));
	vericode_->text_box().set_maximum_chars(max_vericode_size);
	ss.str("");
	ss << _("Verification code");
	vericode_->text_box().set_placeholder(ss.str());

	// password
	password_.reset(new ttext_box2(*window_, *find_widget<tcontrol>(window_, "password", false, true), null_str, "misc/password.png", true, "misc/eye.png", false));
	connect_signal_mouse_left_click(
		password_->button()
		, boost::bind(
			&tresetpwd::clear_password
			, this
			, boost::ref(*window_)));

	password_->text_box().set_maximum_chars(22);
	password_->text_box().set_placeholder(_("Password"));
	password_->set_did_text_changed(boost::bind(&tresetpwd::did_field_changed, this));
	// connect_signal_pre_key_press(password_.text_box(), boost::bind(&tresetpwd::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));

	tgrid& bonus_grid = find_widget<tgrid>(window_, "bonus_grid", false);
	bonus_grid.set_visible(twidget::INVISIBLE);
}

void tresetpwd::post_show()
{
	vericode_timer_.reset();
}

void tresetpwd::set_reset_active(const std::string& mobile, const std::string& vericode, const std::string& password)
{
	bool active = !mobile.empty() && utils::isinteger(mobile);
	if (active) {
		active = !password.empty();
	}
	if (active) {
		active = check_vericode_size(vericode.size());
	}

	register_->set_active(active);
}

void tresetpwd::set_vericode_active(const std::string& mobile)
{
	get_vericode_->set_active(!next_sms_ticks_ && !mobile.empty() && utils::isinteger(mobile));
}

void tresetpwd::did_field_changed()
{
	set_reset_active(mobile_->text_box().label(), vericode_->text_box().label(), password_->text_box().label());
	set_vericode_active(mobile_->text_box().label());
}

void tresetpwd::clear_password(twindow& window)
{
	password_->text_box().set_cipher(!password_->text_box().cipher());
}

void tresetpwd::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

void tresetpwd::user_agreement(twindow& window)
{
}

void tresetpwd::click_get_vericode()
{
	bool ret = run_with_progress(null_str, boost::bind(&net::do_vericode, mobile_->text_box().label(), vericode_password), false, 2000, false);

	if (ret) {
		get_vericode_->set_active(false);
		next_sms_ticks_ = SDL_GetTicks() + vericode_reserve_secs * 1000;
	}
}

void tresetpwd::click_reset()
{
	std::string mobile = mobile_->text_box().label();
	std::string password = password_->text_box().label();

	tuser user;
	bool ret = run_with_progress(null_str, boost::bind(&net::do_resetpwd, mobile, password, vericode_->text_box().label()), false, 2000, false);

	if (ret) {
		// switch to sesame account
		current_user = user;
		preferences::set_startup_login(true);

		window_->set_retval(twindow::OK);
	}
}

void tresetpwd::vericode_timer_handler()
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
		set_vericode_active(mobile_->text_box().label());
	}
}

void tresetpwd::signal_handler_sdl_key_down(bool& handled
		, bool& halt
		, const SDL_Keycode key
		, SDL_Keymod modifier
		, const Uint16 unicode)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if (key == SDLK_PRINTSCREEN) {
		tcontrol* ceil = find_widget<tcontrol>(window_, "login_ceil", false, true);
		ceil->set_visible(twidget::INVISIBLE);
	}
#endif

#ifdef _WIN32
	if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) && !(modifier & KMOD_SHIFT)) {
		handled = true;
		halt = true;
	}
#else
	if (key == SDLK_RETURN) {
		tcontrol* ceil = find_widget<tcontrol>(window_, "login_ceil", false, true);
		ceil->set_visible(twidget::VISIBLE);

		handled = true;
		halt = true;
	}
#endif
}

} // namespace gui2

