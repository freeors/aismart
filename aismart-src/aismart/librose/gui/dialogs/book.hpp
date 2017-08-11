/* $Id: player_selection.hpp 49605 2011-05-22 17:56:24Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_BOOK_HPP_INCLUDED
#define GUI_DIALOGS_BOOK_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include "config.hpp"
#include <set>
#include <list>
#include "help.hpp"

class hero_map;

namespace gui2 {

class ttree_node;
class twindow;
class ttoggle_button;
class ttree;

class tbook : public tdialog
{
public:
	struct tcookie {
		tcookie(const help::topic* t, ttree_node* node)
			: t(t)
			, node(node)
		{}

		const help::topic* t;
		ttree_node* node;
	};

	explicit tbook(tmap* map, hero_map& heros, const config& game_config, const std::string& tag);
	~tbook();

	virtual std::vector<help::topic> generate_topics(const bool sort_generated, const std::string &generator);
	virtual void generate_sections(const config* help_cfg, const std::string &generator, help::section &sec, int level);

protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void technology_toggled(twidget& widget);
	void section_2_tv_internal(ttree_node& htvroot, const help::section& parent);

	void ref_at(twindow& window);
	ttree_node* cookie_rfind_node(const help::topic* t) const;
	void switch_to_topic(twindow& window, const help::topic& t);

protected:
	tmap* map_;
	hero_map& heros_;
	const config& game_config_;
	const std::string tag_;

	ttree* tree_;
	std::map<int, tcookie> cookies_;
	int increase_id_;
	const help::topic* current_topic_;
};

}

#endif

