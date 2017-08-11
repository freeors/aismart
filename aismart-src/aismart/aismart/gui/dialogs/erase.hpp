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

#ifndef GUI_DIALOGS_ERASE_HPP_INCLUDED
#define GUI_DIALOGS_ERASE_HPP_INCLUDED

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
class terase : public tdialog
{
public:
	terase();

private:

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;
	void post_show() override;

	void did_field_changed();
	void set_retval(twindow& window, int retval);
	void click_erase();
	void click_get_vericode();

	void set_erase_active(const std::string& vericode);
	void set_vericode_active();

	void vericode_timer_handler();

private:
	tbutton* get_vericode_;
	tbutton* erase_;
	std::unique_ptr<ttext_box2> vericode_;
	std::string vericode_str_;

	ttimer vericode_timer_;
	Uint32 next_sms_ticks_;
};

} // namespace gui2

#endif
