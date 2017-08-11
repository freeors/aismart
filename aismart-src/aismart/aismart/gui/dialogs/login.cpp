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

#include "gui/dialogs/login.hpp"

#include "game_config.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/panel.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "help.hpp"
#include "filesystem.hpp"
#include <time.h>
#include "wml_exception.hpp"
#include "net.hpp"
#include "base_instance.hpp"
#include "formula_string_utils.hpp"

#include <boost/bind.hpp>


namespace gui2 {

REGISTER_DIALOG(aismart, login)

tlogin::tlogin()
{
}

void tlogin::pre_show()
{
	window_->set_label("misc/white-background.png");

	tbutton* button = find_widget<tbutton>(window_, "return", false, true);
	button->set_icon("misc/back.png");
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tlogin::set_retval
			, this
			, boost::ref(*window_)
			, (int)RETURN));
	// button->set_visible(twidget::HIDDEN);
	

	login_ = find_widget<tbutton>(window_, "login", false, true);
	login_->set_label(_("Login"));
	login_->set_canvas_variable("border", variant("login2-border"));
	login_->set_active(false);
	connect_signal_mouse_left_click(
		*login_
		, boost::bind(
			&tlogin::login
			, this
			, boost::ref(*window_)));

	button = find_widget<tbutton>(window_, "reset_password", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tlogin::forget_password
			, this
			, boost::ref(*window_)));

	button = find_widget<tbutton>(window_, "register", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&tlogin::set_retval
			, this
			, boost::ref(*window_)
			, (int)REGISTER));

	mobile_.reset(new ttext_box2(*window_, *find_widget<tcontrol>(window_, "username", false, true), null_str, "misc/mobile.png"));
	mobile_->set_did_text_changed(boost::bind(&tlogin::email_changed_callback, this, _1));
	mobile_->text_box().set_maximum_chars(22);
	mobile_->text_box().set_placeholder(_("Mobile"));
	// connect_signal_pre_key_press(mobile_->text_box(), boost::bind(&tlogin::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));

	password_.reset(new ttext_box2(*window_, *find_widget<tcontrol>(window_, "password", false, true), null_str, "misc/password.png", true, "misc/eye.png", false));
	connect_signal_mouse_left_click(
		password_->button()
		, boost::bind(
			&tlogin::click_password
			, this));
	password_->text_box().set_maximum_chars(22);
	password_->text_box().set_placeholder(_("Password"));
	password_->set_did_text_changed(boost::bind(&tlogin::password_changed_callback, this, _1));
	// connect_signal_pre_key_press(password_->text_box(), boost::bind(&tlogin::signal_handler_sdl_key_down, this, _3, _4, _5, _6, _7));


	tcontrol* ceil = find_widget<tcontrol>(window_, "login_ceil", false, true);
	ceil->set_visible(twidget::INVISIBLE);

	const std::string username = group.leader().name();
	mobile_->text_box().set_label(username);
}

void tlogin::post_show()
{
}

void tlogin::set_login_active(const std::string& email, const std::string& password)
{
	login_->set_active(!email.empty() && !password.empty());
}

void tlogin::email_changed_callback(ttext_box& widget)
{
	set_login_active(widget.label(), password_->text_box().label());
}

void tlogin::password_changed_callback(ttext_box& widget)
{
	set_login_active(mobile_->text_box().label(), widget.label());
}

void tlogin::click_password()
{
	password_->text_box().set_cipher(!password_->text_box().cipher());
}

void tlogin::forget_password(twindow& window)
{
	window.set_retval(RESETPWD);
}

void tlogin::set_retval(twindow& window, int retval)
{
	window.set_retval(retval);
}

void tlogin::post_login(int openid_type, const std::string& mobile, const std::string& password)
{
	tuser user;
	run_with_progress(null_str, boost::bind(&net::do_login, mobile, password, false, boost::ref(user)), false, 3000, false);

	if (user.valid()) {
		current_user = user;

		preferences::set_openid_type(openid_type);

		preferences::set_remember_password(true);
		preferences::set_password(password);

		hero& h = group.leader();
		h.set_name(mobile);
		preferences::set_hero(instance->heros(), h);

		preferences::set_startup_login(true);

		window_->set_retval(LOGIN);
	}
}

void tlogin::login(twindow& window)
{
	std::string mobile = mobile_->text_box().label();
	std::string password = password_->text_box().label();
	post_login(0, mobile, password);
}

void tlogin::signal_handler_sdl_key_down(bool& handled
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

