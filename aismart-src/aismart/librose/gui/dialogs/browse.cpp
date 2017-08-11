/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/browse.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "rose_config.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"

// std::tolower
#include <cctype>

namespace gui2 {

REGISTER_DIALOG(rose, browse)

const char tbrowse::path_delim = '/';
const std::string tbrowse::path_delim_str = "/";

tbrowse::tfile2::tfile2(const std::string& name)
	: name(name)
	, method(tbrowse::METHOD_ALPHA)
{
	lower = utils::lowercase(name);
}
bool tbrowse::tfile2::operator<(const tbrowse::tfile2& that) const 
{
	return lower < that.lower; 
}

tbrowse::tbrowse(tparam& param)
	: param_(param)
	, navigate_(NULL)
	, goto_higher_(NULL)
	, filename_(NULL)
	, ok_(NULL)
	, current_dir_(get_path(param.initial))
	, auto_select_(false)
{
	if (!is_directory(current_dir_)) {
		current_dir_ = get_path(game_config::preferences_dir);
	}
}

void tbrowse::pre_show()
{
	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	if (!param_.title.empty()) {
		label->set_label(param_.title);
	}

	filename_ = find_widget<ttext_box>(window_, "filename", false, true);
	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	if (!param_.open.empty()) {
		ok_->set_label(param_.open);
	}

	goto_higher_ = find_widget<tbutton>(window_, "goto-higher", false, true);
	connect_signal_mouse_left_click(
			  *goto_higher_
			, boost::bind(
				&tbrowse::goto_higher
				, this
				, boost::ref(*window_)));

	navigate_ = find_widget<treport>(window_, "navigate", false, true);
	navigate_->set_did_item_click(boost::bind(&tbrowse::click_navigate, this, boost::ref(*window_), _2));
	// reload_navigate(window, true);

	if (param_.readonly) {
		filename_->set_active(false);
	}
	filename_->set_did_text_changed(boost::bind(&tbrowse::text_changed_callback, this, _1));

	tlistbox& list = find_widget<tlistbox>(window_, "default", false);
	if (game_config::mobile) {
		list.enable_select(false);
	} else {
		list.set_did_row_double_click(boost::bind(&tbrowse::item_double_click, this, boost::ref(*window_), _1, _2));
	}
	list.set_did_row_changed(boost::bind(&tbrowse::item_selected, this, boost::ref(*window_), _1, _2));

	window_->keyboard_capture(&list);

	// update_file_lists(window);
	init_entry(*window_);
}

void tbrowse::post_show()
{
	param_.result = calculate_result();
}

std::string tbrowse::calculate_result() const
{
	const std::string& label = filename_->label();
	if (current_dir_ != path_delim_str) {
		return current_dir_ + path_delim + label;
	} else {
		return current_dir_ + label;
	}
}

void tbrowse::init_entry(twindow& window)
{
	entries_.push_back(tentry(game_config::preferences_dir, _("My Document"), "misc/documents.png", "document"));
#ifdef _WIN32
	entries_.push_back(tentry("c:", _("C:"), "misc/disk.png", "device1"));
#else
	entries_.push_back(tentry("/", _("Device"), "misc/disk.png", "device1"));
#endif
	entries_.push_back(tentry(game_config::path, _("Sandbox"), "misc/dir-res.png", "device2"));
	if (param_.extra.valid()) {
		entries_.push_back(param_.extra);
		entries_.back().id = "extra";
	}

	tlistbox& root = find_widget<tlistbox>(&window, "root", false);
	std::map<std::string, std::string> data;

	int index = 0;
	for (std::vector<tentry>::const_iterator it = entries_.begin(); it != entries_.end(); ++ it, index ++) {
		const tentry& entry = *it;
		data.clear();
		data.insert(std::make_pair("label", entry.label));
		ttoggle_panel& row = root.insert_row(data);
		row.set_child_icon("label", entry.icon);
	}
	root.set_did_row_changed(boost::bind(&tbrowse::did_root_changed, this, boost::ref(window), _1, _2));
	root.select_row(0);
}

void tbrowse::reload_navigate(twindow& window, bool first)
{
	const gui2::tgrid::tchild* children = navigate_->child_begin();
	int childs = navigate_->items();

	size_t prefix_chars;
	std::string adjusted_current_dir;
	std::vector<std::string> vstr;

	if (current_dir_ != path_delim_str) {
#ifdef _WIN32
		// prefix_chars = 1;
		// adjusted_current_dir = path_delim_str + current_dir_;

		// this version don't support change volume in table. so easy market above. use below temporary.
		// Should modify filesystem function about Win32 in furture, it requrie use UNICODE.
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
#else
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
#endif
		vstr = utils::split(adjusted_current_dir, path_delim, utils::STRIP_SPACES);
	} else {
		prefix_chars = 0;
		adjusted_current_dir = current_dir_;
		vstr.push_back(path_delim_str);
	}

	int vsize = (int)vstr.size();
	std::vector<std::string> ids;
	int start_sect = 0;

	int can_disp_count = 4;
	if (first) {
		const int initial_count = 3;
		can_disp_count = initial_count;
	}
	if (vsize > can_disp_count) {
		start_sect = vsize - can_disp_count;
		ids.push_back("...<<");
		for (int n = vsize - can_disp_count + 1; n < vsize; n ++) {
			ids.push_back(vstr[n]);
		}
	} else {
		ids = vstr;
	}

	while (childs > (int)ids.size()) {
		// require reverse erase.
		navigate_->erase_item(-- childs);
	}
	
	int start_pos = -1;
	cookie_paths_.clear();
	while (start_sect --) {
		start_pos = adjusted_current_dir.find(path_delim, start_pos + 1);
	}

	childs = navigate_->items();
	int n = 0;
	gui2::tbutton* widget;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it, n ++) {
		if (n < childs) {
			widget = dynamic_cast<tbutton*>(children[n].widget_);
			widget->set_label(*it);
		} else {
			tcontrol& widget2 = navigate_->insert_item(null_str, *it);
			widget2.set_cookie(n);
		}
		start_pos = adjusted_current_dir.find(path_delim, start_pos + 1);
		if (start_pos) {
			cookie_paths_.push_back(adjusted_current_dir.substr(prefix_chars, start_pos - prefix_chars));
		} else {
			// like unix system. first character is path_delim.
			cookie_paths_.push_back(path_delim_str);
			widget->set_label(cookie_paths_.back());
		}
	}

	goto_higher_->set_active(ids.size() >= 2);
}

void tbrowse::click_navigate(twindow& window, tbutton& widget)
{
	current_dir_ = cookie_paths_[widget.at()];
	update_file_lists(window);

	reload_navigate(window, false);
}

void tbrowse::did_root_changed(twindow& window, tlistbox& list, ttoggle_panel& widget)
{
	current_dir_ = entries_[widget.at()].path;
	update_file_lists(window);

	reload_navigate(window, false);
}

const std::string& get_browse_icon(bool dir)
{
	static const std::string dir_icon = "misc/folder.png";
	static const std::string file_icon = "misc/file.png";
	return dir? dir_icon: file_icon;
}

void tbrowse::add_row(twindow& window, tlistbox& list, const std::string& name, bool dir)
{
	std::map<std::string, std::string> list_item_item;

	list_item_item.insert(std::make_pair("name", name));

	list_item_item.insert(std::make_pair("date", "---"));

	list_item_item.insert(std::make_pair("size", dir? null_str: "---"));

	ttoggle_panel& widget = list.insert_row(list_item_item);
	widget.set_child_icon("name", get_browse_icon(dir));
}

void tbrowse::reload_file_table(twindow& window, int cursel)
{
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	int size = int(dirs_in_current_dir_.size() + files_in_current_dir_.size());
	for (std::set<tfile2>::const_iterator it = dirs_in_current_dir_.begin(); it != dirs_in_current_dir_.end(); ++ it) {
		const tfile2& file = *it;
		add_row(window, *list, file.name, true);

	}
	for (std::set<tfile2>::const_iterator it = files_in_current_dir_.begin(); it != files_in_current_dir_.end(); ++ it) {
		const tfile2& file = *it;
		add_row(window, *list, file.name, false);
	}
	if (size) {
		if (cursel >= size) {
			cursel = size - 1;
		}
		tauto_select_lock lock(*this);
		list->select_row(cursel);
	}
}

void tbrowse::update_file_lists(twindow& window)
{
	files_in_current_dir_.clear();
	dirs_in_current_dir_.clear();

	std::vector<std::string> files, dirs;
	get_files_in_dir(current_dir_, &files, &dirs, FILE_NAME_ONLY);

	// files and dirs of get_files_in_dir returned are unicode16 format
	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& str = *it;
		files_in_current_dir_.insert(str);
	}
	for (std::vector<std::string>::const_iterator it = dirs.begin(); it != dirs.end(); ++ it) {
		const std::string& str = *it;
		dirs_in_current_dir_.insert(str);
	}

	reload_file_table(window, 0);
}

void tbrowse::open(twindow& window, const int at)
{
	std::set<tfile2>::const_iterator it = dirs_in_current_dir_.begin();
	std::advance(it, at);

	if (current_dir_ != path_delim_str) {
		current_dir_ = current_dir_ + path_delim + it->name;
	} else {
		current_dir_ = current_dir_ + it->name;
	}
	update_file_lists(window);

	reload_navigate(window, false);
}

void tbrowse::goto_higher(twindow& window)
{
	size_t pos = current_dir_.rfind(path_delim);
	if (pos == std::string::npos) {
		goto_higher_->set_active(false);
		return;
	}

	current_dir_ = current_dir_.substr(0, pos);
	update_file_lists(window);

	reload_navigate(window, false);
}

std::string tbrowse::get_path(const std::string& file_or_dir) const 
{
	std::string res_path = file_or_dir;
	std::replace(res_path.begin(), res_path.end(), '\\', path_delim);

	// get rid of all path_delim at end.
	size_t s = res_path.size();
	while (s > 1 && res_path.at(s - 1) == path_delim) {
		res_path.erase(s - 1);
		s = res_path.size();
	}

	if (!::is_directory(file_or_dir)) {
		size_t index = file_or_dir.find_last_of(path_delim);
		if (index != std::string::npos) {
			res_path = file_or_dir.substr(0, index);
		}
	}
	return res_path;
}

void tbrowse::item_selected(twindow& window, tlistbox& list, ttoggle_panel& widget)
{
	bool dir = false;

	std::set<tfile2>::const_iterator it;
	int row = widget.at();
	if (row < (int)dirs_in_current_dir_.size()) {
		dir = true;
		it = dirs_in_current_dir_.begin();
#ifdef _WIN32
#else
		if (!auto_select_) {
			open(window, row);
			return;
		}
#endif
	} else {
		it = files_in_current_dir_.begin();
		row -= dirs_in_current_dir_.size();
	}
	std::advance(it, row);
	selected_ = it->name;

	set_filename(window);
}

void tbrowse::item_double_click(twindow& window, tlistbox& list, ttoggle_panel& widget)
{
	int row = widget.at();
	if (row >= (int)dirs_in_current_dir_.size()) {
		return;
	}

	open(window, row);
}

void tbrowse::text_changed_callback(ttext_box& widget)
{
	const std::string& label = widget.label();
	bool active = !label.empty();
	if (active) {
		const std::string path = calculate_result();
		if (SDL_IsFile(path.c_str())) {
			if (!(param_.flags & TYPE_FILE)) {
				active = false;
			}
		} else if (SDL_IsDirectory(path.c_str())) {
			if (!(param_.flags & TYPE_DIR)) {
				active = false;
			}
		}
		if (active && did_result_changed_) {
			active = did_result_changed_(calculate_result(), label);
		}
	}
	ok_->set_active(active);
}

void tbrowse::set_filename(twindow& window)
{
	filename_->set_label(selected_);
}

}
