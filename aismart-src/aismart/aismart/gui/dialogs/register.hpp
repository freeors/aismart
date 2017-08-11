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

#ifndef GUI_DIALOGS_REGISTER_HPP_INCLUDED
#define GUI_DIALOGS_REGISTER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/text_box2.hpp"
#include "gui/widgets/timer.hpp"


namespace gui2 {

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 *
 * @todo Evaluate whether we can handle more buttons in this class.
 */
class tregister : public tdialog
{
public:
	tregister();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void clear_password(twindow& window);
	void did_field_changed();
	void set_retval(twindow& window, int retval);
	void click_register();
	void click_get_vericode();

	void set_register_active(const std::string& mobile, const std::string& email, const std::string& password);
	void set_vericode_active(const std::string& mobile);
	void signal_handler_sdl_key_down(bool& handled, bool& halt, const SDL_Keycode key, SDL_Keymod modifier, const Uint16 unicode);

	void user_agreement(twindow& window);

	void vericode_timer_handler();

private:
	tbutton* get_vericode_;
	tbutton* register_;
	std::unique_ptr<ttext_box2> mobile_;
	std::unique_ptr<ttext_box2> password_;
	std::unique_ptr<ttext_box2> vericode_;
	std::string vericode_str_;

	ttimer vericode_timer_;
	Uint32 next_sms_ticks_;
};

} // namespace gui2

#endif
