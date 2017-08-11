/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
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

#ifndef GUI_DIALOGS_COMBO_BOX_HPP_INCLUDED
#define GUI_DIALOGS_COMBO_BOX_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class tlistbox;
class ttoggle_panel;

class tcombo_box : public tdialog
{
public:
	explicit tcombo_box(const std::vector<std::string>& items, int initial_sel, const std::string& title = null_str, bool allow_cancel = false);

	/**
	 * Returns the selected item index after displaying.
	 * @return -1 if the dialog was cancelled.
	 */
	int cursel() const { return cursel_; }
	bool dirty() const { return cursel_ != initial_sel_; }

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void item_selected(twindow& window, tlistbox& list, ttoggle_panel& widget);

private:
	const int initial_sel_;
	int cursel_;
	std::vector<std::string> items_;
	const std::string title_;
	bool allow_cancel_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
