/* $Id: simple_item_selector.hpp 48440 2011-02-07 20:57:31Z mordante $ */
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

#ifndef GUI_DIALOGS_MULTIPLE_SELECTOR_HPP_INCLUDED
#define GUI_DIALOGS_MULTIPLE_SELECTOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/listbox.hpp"

#include <vector>

namespace gui2 {

class tbutton;

class tmultiple_selector : public tdialog
{
public:
	tmultiple_selector(const std::string& title, const std::vector<std::string>& items, const std::set<int>& initial_selected = std::set<int>());

	const std::set<int>& selected() const { return selected_; }

	// states_ has involved this widget's state.
	void set_did_selector_changed(const boost::function<bool (ttoggle_button&, const int, const std::vector<bool>& )>& did)
	{
		did_selector_changed_ = did;
	}

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void did_selector_changed(ttoggle_button& widget, int at);
private:
	std::string title_;
	const std::vector<std::string>& items_;	
	const std::set<int>& initial_selected_;
	std::vector<bool> states_;
	boost::function<bool (ttoggle_button&, const int, const std::vector<bool>& )> did_selector_changed_;

	std::set<int> selected_;
	tbutton* ok_;
};

}

#endif /* ! GUI_DIALOGS_MULTIPLY_SELECTOR_HPP_INCLUDED */
