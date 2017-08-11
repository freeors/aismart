/* $Id: title_screen.hpp 48740 2011-03-05 10:01:34Z mordante $ */
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

#ifndef GUI_DIALOGS_LOGIN_HPP_INCLUDED
#define GUI_DIALOGS_LOGIN_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/text_box2.hpp"


namespace gui2 {

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tlogin : public tdialog
{
public:
	enum tresult {RETURN = 1, LOGIN, REGISTER, RESETPWD};

	tlogin();

	void post_login(int openid_type, const std::string& mobile, const std::string& password);
private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void click_password();
	void email_changed_callback(ttext_box& widget);
	void password_changed_callback(ttext_box& widget);
	void forget_password(twindow& window);
	void set_retval(twindow& window, int retval);
	void login(twindow& window);

	void set_login_active(const std::string& email, const std::string& password);
	void signal_handler_sdl_key_down(bool& handled, bool& halt, const SDL_Keycode key, SDL_Keymod modifier, const Uint16 unicode);

	// bool did_progress_login(const std::string& mobile, const std::string& password, tuser& user);

private:
	tbutton* login_;
	std::unique_ptr<ttext_box2> mobile_;
	std::unique_ptr<ttext_box2> password_;
};

} // namespace gui2

#endif
