/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/book.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_panel.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "map.hpp"
#include "integrate.hpp"
#include "font.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

REGISTER_DIALOG(rose, book)

help::section toplevel;

tbook::tbook(tmap* map, hero_map& heros, const config& game_config, const std::string& tag)
	: map_(map)
	, heros_(heros)
	, game_config_(game_config)
	, tag_(tag)
	, cookies_()
	, increase_id_(0)
	, current_topic_(NULL)
{
	int screen_width = settings::screen_width;
	int screen_height = settings::screen_height;

	if (screen_width < 640 || screen_height < 480) {
		tintegrate::screen_ratio = 2;
	} else if (screen_width < 800 || screen_height < 600) {
		tintegrate::screen_ratio = 1.5;
	}
	help::book_toplevel = &toplevel;
}

tbook::~tbook()
{
	help::book_toplevel = NULL;

	tintegrate::screen_ratio = 1;
	toplevel.clear();
	help::clear_book();
}

void tbook::technology_toggled(twidget& widget)
{
	ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(&widget);
	int cookie = toggle->get_data();

	const help::topic* t = cookies_.find(cookie)->second.t;
	switch_to_topic(*window_, *t);
}

void tbook::pre_show()
{
	help::init_book(&game_config_, map_, false);

	const config *help_config = &game_config_.find_child("book", "id", tag_);
	if (!*help_config) {
		help_config = &help::dummy_cfg;
	}

	std::stringstream strstr;
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (help_config->has_attribute("title")) {
		strstr << help_config->get("title")->str();
	}
	label->set_label(strstr.str());

	help::generate_contents(this, tag_, toplevel);

	ttree& parent_tree = find_widget<ttree>(window_, "default", false);
	tree_ = &parent_tree;

	section_2_tv_internal(tree_->get_root_node(), toplevel);
	if (!cookies_.empty()) {
		const std::string default_topic_id = "title";
		const help::topic* t = find_topic(toplevel, default_topic_id);
		if (!t) {
			t = cookies_.find(0)->second.t;
		}
		if (t) {
			switch_to_topic(*window_, *t);
			tree_->select_node(cookie_rfind_node(t));

		}
	}

	tscroll_panel& content = find_widget<tscroll_panel>(window_, "scroll_panel", false);
	content.connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &tbook::ref_at
				  , this
				  , boost::ref(*window_))
			, event::tdispatcher::front_pre_child);

	tree_->get_root_node().fold_children();
}

ttree_node* tbook::cookie_rfind_node(const help::topic* t) const
{
	for (std::map<int, tcookie>::const_iterator it = cookies_.begin(); it != cookies_.end(); ++ it) {
		const tcookie& cookie = it->second;
		if (cookie.t == t) {
			return cookie.node;
		}
	}
	return NULL;
}

void tbook::switch_to_topic(twindow& window, const help::topic& t)
{
	tlabel* content = find_widget<tlabel>(&window, "content", false, true);
	content->set_label(t.text.parsed_text());

	current_topic_ = &t;
}

void tbook::ref_at(twindow& window)
{
	tlabel& label = find_widget<tlabel>(&window, "content", false);
	tintegrate integrate(current_topic_->text.parsed_text(), label.get_width(), label.get_text_font_size(), font::NORMAL_COLOR, false, false);

	int mousex, mousey;
	SDL_GetMouseState(&mousex,&mousey);

	const std::string ref = integrate.ref_at(mousex - label.get_x(), mousey - label.get_y());
	if (!ref.empty()) {
		const help::topic* t = find_topic(toplevel, ref);
		tree_->select_node(cookie_rfind_node(t));

		switch_to_topic(*window_, *t);
	}
}

void tbook::post_show()
{
}

void tbook::section_2_tv_internal(ttree_node& htvroot, const help::section& parent)
{
	std::stringstream strstr;
	std::map<std::string, std::string> tree_group_item;

	for (std::vector<help::section *>::const_iterator it = parent.sections.begin(); it != parent.sections.end(); ++ it) {
		const help::section& sec = **it;

		strstr.str("");
		strstr << sec.title;
		tree_group_item["text"] = strstr.str();
		ttree_node& htvi = htvroot.insert_node("item", tree_group_item);

		ttoggle_panel& toggle = htvi;
		toggle.set_did_state_changed(boost::bind(&tbook::technology_toggled, this, _1));
		toggle.set_data(increase_id_);
		cookies_.insert(std::make_pair(increase_id_ ++, tcookie(find_topic(toplevel, help::section_topic_prefix + sec.id), &htvi)));

		htvi.find("icon", true)->set_visible(twidget::INVISIBLE);

		if (!sec.sections.empty() || !sec.topics.empty()) {
			section_2_tv_internal(htvi, sec);
		}
	}

	for (std::list<help::topic>::const_iterator it = parent.topics.begin(); it != parent.topics.end(); ++ it) {
		const help::topic& t = *it;

		if (t.id.find(help::section_topic_prefix) == 0) {
			continue;
		}

		strstr.str("");
		strstr << t.title;
		tree_group_item["text"] = strstr.str();
		ttree_node& htvi = htvroot.insert_node("item", tree_group_item);

		ttoggle_panel& toggle = htvi;
		toggle.set_did_state_changed(boost::bind(&tbook::technology_toggled, this, _1));
		toggle.set_data(increase_id_);
		cookies_.insert(std::make_pair(increase_id_ ++, tcookie(&t, &htvi)));
	}
}

std::vector<help::topic> tbook::generate_topics(const bool sort_generated, const std::string &generator)
{
	std::vector<help::topic> res;
	return res;
}

void tbook::generate_sections(const config *help_cfg, const std::string &generator, help::section &sec, int level)
{
	return;
}

} // namespace gui2