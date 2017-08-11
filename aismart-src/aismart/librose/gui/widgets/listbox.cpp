/* $Id: listbox.cpp 54521 2012-06-30 17:46:54Z mordante $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/listbox.hpp"

#include "gui/auxiliary/widget_definition/listbox.hpp"
#include "gui/auxiliary/window_builder/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/button.hpp"

#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/image.hpp"

#include "formula_string_utils.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(listbox)

tlistbox::tlistbox(const std::vector<tlinked_group>& linked_groups)
	: tscroll_container(2, true) // FIXME magic number
	, list_builder_(NULL)
	, did_row_changed_(NULL)
	, list_grid_(NULL)
	, cursel_(nullptr)
	, selectable_(true)
	, use_drag_(true)
	, drag_judge_at_(nposm)
	, drag_at_(nposm)
	, drag_offset_(0)
	, left_drag_grid_(nullptr)
	, left_drag_grid_size_(0, 0)
	, left_drag_spacer_(nullptr)
	, longpress_at_(nposm)
	, longpress_ticks_(0)
	, last_longpress_at_(nposm)
	, explicit_select_(false)
	, linked_max_size_changed_(false)
	, row_layout_size_changed_(false)
{
	for (std::vector<tlinked_group>::const_iterator it = linked_groups.begin(); it != linked_groups.end(); ++ it) {
		const tlinked_group& lg = *it;
		if (has_linked_size_group(lg.id)) {
			utils::string_map symbols;
			symbols["id"] = lg.id;
			t_string msg = vgettext2("Linked '$id' group has multiple definitions.", symbols);

			VALIDATE(false, msg);
		}
		init_linked_size_group(lg.id, lg.fixed_width, lg.fixed_height);
	}
}

tlistbox::~tlistbox()
{
	if (left_drag_grid_) {
		delete left_drag_grid_;
	}
	if (left_drag_spacer_) {
		delete left_drag_spacer_;
	}
}

ttoggle_panel& tlistbox::insert_row(const std::map<std::string, std::string>& data, int at)
{
	tdisable_invalidate_layout_lock lock;

	if (gc_locked_at_ != nposm) {
		// same as erase_row.
		// app maybe call insert_row/erase_row continue, and them will set gc_locked_at_.
		// since insert_row/erase_row should udpate gc_locked_at_ always, here set nposm first.
		gc_locked_at_ = nposm;
	}

	if (at != nposm) {
		const int rows = list_grid_->children_vsize();
		VALIDATE(at >= 0 && at < rows, null_str);
	}
	ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(list_builder_->widgets[0]->build());
	widget->set_did_mouse_enter_leave(boost::bind(&tlistbox::did_focus_changed, this, _1, _2));
	widget->set_did_state_pre_change(boost::bind(&tlistbox::did_pre_change, this, _1));
	widget->set_did_state_changed(boost::bind(&tlistbox::did_changed, this, _1));
	widget->set_did_double_click(boost::bind(&tlistbox::did_double_click, this, _1));

	widget->set_child_members(data);
	widget->at_ = list_grid_->listbox_insert_child(*widget, at);
	at = widget->at_; // at maybe nposm

	if (gc_current_content_width_ > 0 && ((at >= gc_first_at_ && at <= gc_last_at_) || (gc_next_precise_at_ > 0 && at <= gc_next_precise_at_))) {
		tgrid::tchild* children = list_grid_->children();
		
		// calculate it.
		ttoggle_panel& panel = *dynamic_cast<ttoggle_panel*>(children[at].widget_);
		{
			tclear_restrict_width_cell_size_lock lock;
			panel.get_best_size();
		}
		tpoint size = panel.calculate_best_size_bh(gc_current_content_width_);
		panel.gc_width_ = size.x;
		panel.gc_height_ = size.y;
		panel.twidget::set_size(tpoint(0, 0));
		panel.set_dirty();

		gc_insert_or_erase_row(at, true);

		gc_locked_at_ = at;
	}

	invalidate();

	return *widget;
}

int tlistbox::gc_handle_rows() const
{ 
	return list_grid_->children_vsize(); 
}

tpoint tlistbox::gc_handle_calculate_size(ttoggle_panel& widget, const int width) const
{
	tpoint ret(0, 0);
	{
		tclear_restrict_width_cell_size_lock lock;
		ret = widget.get_best_size();
	}

	if (!gc_calculate_best_size_) {
		ret = widget.calculate_best_size_bh(width);
	}
	return ret;
}

ttoggle_panel* tlistbox::gc_widget_from_children(const int at) const
{
	tgrid::tchild* children = list_grid_->children();
	return dynamic_cast<ttoggle_panel*>(children[at].widget_);
}

int tlistbox::gc_handle_update_height(const int new_height)
{
	SDL_Rect rect = list_grid_->get_rect();
	SDL_Rect content_grid_rect = content_grid_->get_rect();
	const int diff = new_height - rect.h;

	// SDL_Log("mini_handle_gc, [%i, %i], height(%i) - rect.h(%i) = diff(%i)\n", gc_first_at_, gc_last_at_, height, rect.h, diff);

	list_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));
	content_grid_->twidget::place(tpoint(content_grid_rect.x, content_grid_rect.y), tpoint(content_grid_rect.w, content_grid_rect.h + diff));

	return diff;
}

void tlistbox::validate_children_continuous() const
{
	list_grid_->validate_children_continuous();
}

void tlistbox::cancel_drag()
{
	if (left_drag_grid_ && drag_at_ != nposm) {
		drag_end();
	}
	if (longpress_at_ != nposm) {
		longpress_end();
	}
}

void tlistbox::left_drag_grid_set_widget_visible(const std::map<std::string, tvisible>& visibles)
{
	VALIDATE(left_drag_grid_ && !visibles.empty(), null_str);
	// when drag_at_ != nposm, call twidget.set_visible direct.
	VALIDATE(drag_at_ == nposm, null_str);

	tdisable_invalidate_layout_lock lock;
	for (std::map<std::string, tvisible>::const_iterator it = visibles.begin(); it != visibles.end(); ++ it) {
		twidget* widget = left_drag_grid_->find(it->first, false);
		widget->set_visible(it->second);
	}
}

// caller will erase or insert row. havn't erase it, but have inserted it.
void tlistbox::gc_insert_or_erase_row(const int row, const bool insert)
{
	const int rows = list_grid_->children_vsize();
	tgrid::tchild* children = list_grid_->children();

	bool corrected_distance = false;
	if (row < gc_next_precise_at_) {
		// some row height changed in precise rows.
		int next_distance = nposm;
		if (insert) {
			next_distance = 0;
			if (row > 0) {
				ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row - 1].widget_);
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
			gc_next_precise_at_ ++;
		}
		for (int at = row; at < gc_next_precise_at_; at ++) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			if (!insert || at != row) {
				VALIDATE(gc_is_precise(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
			} else {
				VALIDATE(widget->gc_distance_ == nposm && widget->gc_height_ != nposm, null_str);
			}
			if (at == row) {
				// row will erase or invisible.
				if (insert) {
					widget->gc_distance_ = next_distance;
					next_distance = widget->gc_distance_ + widget->gc_height_;
				} else {
					next_distance = widget->gc_distance_;
				}
			} else {
				widget->gc_distance_ = next_distance;
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
		}

		if (!insert) {
			gc_next_precise_at_ --;
		}
		corrected_distance = true;

	} else if (row == gc_next_precise_at_) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_ - 1].widget_);
		int next_distance = widget->gc_distance_ + widget->gc_height_;
		if (insert) {
			ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
			current_gc_row->gc_distance_ = next_distance;
			gc_next_precise_at_ ++;

		} else {
			for (++ gc_next_precise_at_; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
				ttoggle_panel* current_gc_row = dynamic_cast<ttoggle_panel*>(children[gc_next_precise_at_].widget_);
				if (current_gc_row->gc_height_ == nposm) {
					break;
				} else {
					VALIDATE(!gc_is_precise(current_gc_row->gc_distance_), null_str);
					current_gc_row->gc_distance_ = next_distance;
				}
				next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
			}
			gc_next_precise_at_ --;
		}
	}

	// session2: set gc_first_at_, gc_last_at
	if (row < gc_first_at_) {
		if (insert) {
			gc_first_at_ ++;
			gc_last_at_ ++;
		} else {
			gc_first_at_ --;
			gc_last_at_ --;
		}

	} else if (row >= gc_first_at_ && row <= gc_last_at_) {
		if (gc_first_at_ == gc_last_at_) {
			if (insert) {
				gc_first_at_ ++;
				gc_last_at_ ++;
			} else {
				gc_first_at_ = gc_last_at_ = nposm;
			}

		} else if (insert) {
			// of couse, can fill toggle_panel of row, and expand [gc_first_at_, gc_last_at_].
			// but if use it, if app insert in [gc_first_at_, gc_last_at_] conitnue, it will slower and slower!
			// so use decrease [gc_first_at_, gc_last_at_].
			if (row == gc_first_at_) {
				gc_first_at_ ++;
				gc_last_at_ ++;
			} else {
				// insert. |->row   row<-|
				const int diff_first = row - gc_first_at_;
				const int diff_last = gc_last_at_ - row;
				if (diff_first < diff_last) {
					for (int at = gc_first_at_; at < row; at ++) {
						ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
						garbage_collection(*widget);
					}
					VALIDATE(row < gc_last_at_, null_str);
					gc_first_at_ = row + 1;

				} else {
					for (int at = row + 1; at <= gc_last_at_ + 1; at ++) {
						ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
						garbage_collection(*widget);
					}

					VALIDATE(row > gc_first_at_, null_str);
					gc_last_at_ = row - 1;
				}
			}
			
		} else {
			// erase.
			if (!corrected_distance) {
				int next_distance = nposm;
				for (int at = row; at <= gc_last_at_; at ++) {
					ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
					VALIDATE(gc_is_estimated(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
					if (at == row) {
						// row will erase or invisible.
						next_distance = gc_distance_value(widget->gc_distance_);
					} else {
						widget->gc_distance_ = gc_join_estimated(next_distance);
						next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;
					}
				}
			}
			gc_last_at_ --;
		}
	}
}

void tlistbox::erase_row(int at)
{
	if (gc_locked_at_ != nposm) {
		// same as insert_row.
		// app maybe call erase_row continue, and erase_row will set gc_locked_at_.
		// 1. after erase_row, there is only one row in listox, so set gc_locked_at_ to 0.
		// 2. next erase_row, only one row will be erase. it should set gc_locked_at_ to nposm.
		//    since erase_row should udpate gc_locked_at_ always, here set nposm first.
		gc_locked_at_ = nposm;
	}

	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == nposm) {
		at = rows - 1;
	}
	VALIDATE(at >= 0 && at < rows, null_str);

	if (left_drag_grid_ && drag_at_ != nposm && drag_at_ <= at) {
		// end drag require right at_, so execute before erase operator.
		drag_end();
	}
	if (longpress_at_ == at) {
		longpress_end();
	}

	const bool auto_select = true;
	bool require_reselect = selectable_ && auto_select && (cursel_ && cursel_->at_ == at);

	gc_insert_or_erase_row(at, false);

	list_grid_->listbox_erase_child(at);

	// update subsequent panel's at_.
	tgrid::tchild* children = list_grid_->children();
	const int post_rows = list_grid_->children_vsize();

	if (post_rows && require_reselect) {
		ttoggle_panel* new_at = next_selectable(at, true);
		if (new_at) {
			select_row(new_at->at_);
		}
	}

	if (gc_current_content_width_ > 0 && gc_first_at_ != nposm) {
		// caller maybe call erase_row continue, will result to large effect burden
		if (at < post_rows) {
			gc_locked_at_ = at;
		} else if (post_rows) {
			gc_locked_at_ = at - 1;
		}
	}
	invalidate();
}

void tlistbox::clear()
{
	if (left_drag_grid_ && drag_at_ != nposm) {
		// end drag require right at_, so execute before erase operator.
		drag_end();
	}

	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}

	for (int at = 0; at < rows; at ++) {
		list_grid_->listbox_erase_child(nposm);
	}

	reset();

	invalidate();
}

class tsort_func
{
public:
	tsort_func(const boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)>& did_compare)
		: did_compare_(did_compare)
	{}

	bool operator()(ttoggle_panel* a, ttoggle_panel* b) const
	{
		return did_compare_(*a, *b);
	}

private:
	boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)> did_compare_;
};

void tlistbox::sort(const boost::function<bool (const ttoggle_panel& widget, const ttoggle_panel&)>& did_compare)
{
	tgrid::tchild* children = list_grid_->children();
	const int rows = list_grid_->children_vsize();

	if (rows < 2) {
		return;
	}

	int align_row = nposm;
	if (gc_current_content_width_ > 0 && gc_first_at_ != nposm) {
		const int item_position = vertical_scrollbar_->get_item_position();
		for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
			ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			if (gc_distance_value(panel->gc_distance_) >= item_position) {
				align_row = at;
				break;
			}
		}
	}

	if (gc_first_at_ != nposm) {
		for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
			ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			garbage_collection(*panel);
		}
		gc_first_at_ = gc_last_at_ = nposm;
	}
	ttoggle_panel* original_cursel = cursel_;
	bool check_cursel = cursel_ != NULL;

	std::vector<ttoggle_panel*> tmp;
	tmp.resize(rows, NULL);
	for (int n = 0; n < rows; n ++) {
		tmp[n] = dynamic_cast<ttoggle_panel*>(children[n].widget_);
	}
	std::stable_sort(tmp.begin(), tmp.end(), tsort_func(did_compare));
	for (int n = 0; n < rows; n ++) {
		children[n].widget_ = tmp[n];
		ttoggle_panel* panel = tmp[n];
		if (check_cursel && panel->at_ == cursel_->at_) {
			cursel_ = panel;
			check_cursel = false;
		}
		panel->at_ = n;
		panel->gc_distance_ = nposm; // distnace is invalid, reset to nposm.
	}

	gc_next_precise_at_ = 0;
	int next_distance = 0;
	for (int at = 0; at < rows; at ++, gc_next_precise_at_ ++) {
		ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(children[at].widget_);
		if (panel->gc_height_ == nposm) {
			break;
		}
		panel->gc_distance_ = next_distance;
		next_distance = panel->gc_distance_ + panel->gc_height_;

	}

	// if (align_row != nposm && gc_next_precise_at_) {
	if (false) {
		// scroll_to_row maybe result to jittle. so don't use it.
		scroll_to_row(align_row);
	} else {
		invalidate();
	}
}

int tlistbox::rows() const
{
	return list_grid_->children_vsize();
}

void tlistbox::invalidate_layout(twidget* widget)
{
	if (widget) {
		VALIDATE(!widget->float_widget(), null_str);
		// if widget is row(toggle_panel), needn't execute in below if(tmp), so use parent at first.
		twidget* tmp = widget->parent();
		while (tmp) {
			if (tmp == left_drag_grid_) {
				break;
			}
			if (tmp->get_control_type() == "toggle_panel" && tmp->parent()->id() == "_list_grid") {
				break;
			}
			tmp = tmp->parent();
		}
		if (tmp == left_drag_grid_) {
			
		} else if (tmp) {
			ttoggle_panel& panel = *dynamic_cast<ttoggle_panel*>(tmp);
			const int at = panel.at();
			if (gc_first_at_ != nposm && at >= gc_first_at_ && at <= gc_last_at_) {
				{
					tlink_group_owner_lock lock(*this);
					panel.layout_init(false);
				}
				layout_init2();
				scrollbar_moved(true);
			}
		}
	}
	tscroll_container::invalidate_layout(widget);
}

void tlistbox::validate_visible_pre_change(const twidget& widget) const
{
	bool ok = true;
	if (widget.get_control_type() == "toggle_panel" && widget.parent()->id() == "_list_grid") {
		ok = false;
	}
	VALIDATE(ok, "must not set row's toggle_panel visible.");
}

void tlistbox::invalidate(/*const twidget& widget*/)
{
	twindow* window = get_window();
	if (!window->layouted()) {
		return;
	}

	bool require_layout_window = false;
	if (height_is_max_) {
		require_layout_window = true;
	}
	if (!require_layout_window) {
		const tpoint cfg_size = best_size_from_builder();
		if (cfg_size.y == nposm) {
			require_layout_window = true;
		}
	}

	if (require_layout_window) {
		window->invalidate_layout(nullptr);
	} else {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(nullptr);
	}
}

void tlistbox::garbage_collection(ttoggle_panel& row)
{
	row.clear_texture();
}

void tlistbox::show_row_rect(const int at)
{
	const tgrid::tchild* children = list_grid_->children();
	const int rows = list_grid_->children_vsize();

	VALIDATE(at >=0 && at < rows && gc_first_at_ != nposm, null_str);

	const ttoggle_panel* first_widget = dynamic_cast<ttoggle_panel*>(children[gc_first_at_].widget_);
	const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
	SDL_Rect rect{0, gc_distance_value(widget->gc_distance_), 0, widget->gc_height_};
	
	tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	int header_height = header->get_best_size().y;

	// rect.x is distance from list_grid_.
	rect.h += header_height;

	show_content_rect(rect);
	if ((int)vertical_scrollbar_->get_item_position() < gc_distance_value(first_widget->gc_distance_)) {
		// SDL_Log("show_row_rect, item_position(%i) < gc_first_at_'s distance (%i), set item_position to this distance\n", 
		//	at, vertical_scrollbar_->get_item_position(), gc_distance_value(first_widget->gc_distance_));
		vertical_scrollbar_->set_item_position(gc_distance_value(first_widget->gc_distance_));
	}

	// SDL_Log("tlistbox::show_row_rect------post [%i, %i], item_position: %i\n", gc_first_at_, gc_last_at_, vertical_scrollbar_->get_item_position());
	
	if (vertical_scrollbar_ != dummy_vertical_scrollbar_) {
		dummy_vertical_scrollbar_->set_item_position(vertical_scrollbar_->get_item_position());
	}

	// Update.
	scrollbar_moved(true);
}

ttoggle_panel& tlistbox::row_panel(const int at) const
{
	const tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();
	VALIDATE(at >= 0 && at < childs, null_str);

	return *dynamic_cast<ttoggle_panel*>(children[at].widget_);
}

bool tlistbox::select_row(const int at)
{
	ttoggle_panel* desire_widget = nullptr;
	if (at != nposm) {
		const tgrid::tchild* children = list_grid_->children();
		const int childs = list_grid_->children_vsize();
		VALIDATE(at >= 0 && at < childs, null_str);

		if (children[at].widget_ == cursel_) {
			return true;
		}
		desire_widget = &row_panel(at);
	}

	texplicit_select_lock lock(*this);
	return select_internal(desire_widget);
}

void tlistbox::enable_select(const bool enable)
{
	if (enable == selectable_) {
		return;
	}
	selectable_ = enable;
	if (enable) {
		VALIDATE(!cursel_, null_str);
	} else {
		select_internal(nullptr);
	}
}

// @widget: maybe null.
bool tlistbox::select_internal(twidget* widget, const bool selected)
{
	ttoggle_panel* desire_panel = nullptr;
	if (widget) {
		desire_panel = dynamic_cast<ttoggle_panel*>(widget);
		VALIDATE(!cursel_ || desire_panel->at_ != cursel_->at_, null_str);
	}

	// first call set_value(true). because did_pre_change require cursel() to get previous.
	ttoggle_panel* original_cursel = cursel_;
	bool changed = true;
	if (desire_panel) {
		if (selectable_ && !selected) {
			changed = desire_panel->set_value(true);
		}

		if (changed) {
			if (selectable_) {
				VALIDATE(desire_panel->get_value(), null_str);
				cursel_ = desire_panel;
			} else {// non-select mode.
				if (selected) {
					// it is called during user click. de-select.
					desire_panel->set_value(false);
				}
			}
		}
	}

	// rule: there is one selectable row at most at any time.
	if (changed && original_cursel) {
		VALIDATE(original_cursel->get_value(), "Previous toggle panel must be selected!");
		original_cursel->set_value(false);
		if (!desire_panel) {
			VALIDATE(original_cursel == cursel_, null_str);
			cursel_ = nullptr;
		}
	}

	if (changed && (cursel_ || (!selectable_ && desire_panel)) && did_row_changed_) {
		int origin_rows = list_grid_->children_vsize();
		did_row_changed_(*this, *desire_panel);
		// did_row_changed_ maybe insert/erase row. for example browse.
		// VALIDATE(list_grid_->children_vsize() == origin_rows, null_str);
	}

	if (!selectable_) {
		VALIDATE(!cursel_, null_str);
	}
	return changed;
}

ttoggle_panel* tlistbox::cursel() const
{
	return cursel_;
}

void tlistbox::did_focus_changed(ttoggle_panel& widget, const bool enter)
{
	if (did_row_focus_changed_) {
		did_row_focus_changed_(*this, widget, enter);
	}
}

bool tlistbox::did_pre_change(ttoggle_panel& widget)
{
	bool allow = true;
	if (selectable_ && did_row_pre_change_) {
		int origin_rows = list_grid_->children_vsize();
		allow = did_row_pre_change_(*this, widget);
		VALIDATE(list_grid_->children_vsize() == origin_rows, null_str);
	}
	return allow;
}

void tlistbox::did_changed(ttoggle_panel& widget)
{
	if (!explicit_select_) {
		select_internal(&widget, true);
	}
}

void tlistbox::did_double_click(ttoggle_panel& widget)
{
	if (did_row_double_click_) {
		did_row_double_click_(*this, widget);
	}
}

void tlistbox::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin)
{
	// Here need call twidget::set_visible_area in conent_grid_ only.
	content_grid()->twidget::set_origin(content_origin);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();

	header->set_origin(tpoint(content_origin.x, origin.y));
	list_grid_->set_origin(tpoint(content_origin.x, content_origin.y + size.y));

	VALIDATE(drag_at_ == nposm && longpress_at_ == nposm, "please call cancel_drag() before popup new window.");
}

void tlistbox::mini_set_content_grid_visible_area(const SDL_Rect& area)
{
	// Here need call twidget::set_visible_area in conent_gri_ only.
	content_grid()->twidget::set_visible_area(area);

	tgrid* header = find_widget<tgrid>(content_grid(), "_header_grid", true, false);
	tpoint size = header->get_size();
	SDL_Rect header_area = intersect_rects(area, header->get_rect());
	header->set_visible_area(header_area);
	
	SDL_Rect list_area = area;
	list_area.y = area.y + size.y;
	list_area.h = area.h - size.y;
	list_grid_->set_visible_area(list_area);
}

void tlistbox::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}

	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		VALIDATE(drag_at_ != nposm, null_str);

		left_drag_grid_->dirty_under_rect(clip);
	}
	tscroll_container::dirty_under_rect(clip);
}

twidget* tlistbox::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	twidget* result = NULL;
	if (left_drag_grid_ && left_drag_grid_->get_visible() == twidget::VISIBLE) {
		VALIDATE(drag_at_ != nposm, null_str);

		tcontrol* widget = dynamic_cast<tcontrol*>(list_grid_->child(drag_at_).widget_);
		result = left_drag_grid_->find_at(coordinate, must_be_active);
	}
	if (!result) {
		result = tscroll_container::find_at(coordinate, must_be_active);
	}
	return result;
}

void tlistbox::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (left_drag_grid_ && drag_at_ != nposm) {
		VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE && left_drag_spacer_->get_visible() == twidget::VISIBLE, null_str);

		{
			std::vector<twidget*> child_call_stack(call_stack);
			child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
			left_drag_grid_->populate_dirty_list(caller, child_call_stack);
		}

		if (left_drag_spacer_->get_drawing_action() != twidget::NOT_DRAWN) {
			std::vector<twidget*> child_call_stack(call_stack);
			child_call_stack.push_back(list_grid_->child(drag_at_).widget_);
			left_drag_spacer_->populate_dirty_list(caller, child_call_stack);
		}
	}

	tscroll_container::child_populate_dirty_list(caller, call_stack);
}

bool tlistbox::popup_new_window()
{
	// new window maybe popup dialog, it require show which row that left-drag in. so don't call cancel_drag().
	return tscroll_container::popup_new_window();
}

void tlistbox::scroll_to_row(int at)
{
	if (gc_current_content_width_ <= 0) {
		// listbox not ready to gc.
		return;
	}

	const int rows = list_grid_->children_vsize();
	if (!rows) {
		return;
	}
	if (at == nposm) {
		at = rows - 1;
	}
	gc_locked_at_ = at;
	mini_handle_gc(horizontal_scrollbar_->get_item_position(), vertical_scrollbar_->get_item_position());
}

tgrid::titerator tlistbox::iterator() const
{
	tgrid::titerator ret{list_grid_->children(), list_grid_->children_vsize()};
	return ret;
}

void tlistbox::layout_init(bool linked_group_only)
{
	// normal flow, do nothing.
}

void tlistbox::layout_linked_widgets(bool dirty)
{
	for (std::map<std::string, tlinked_size>::iterator it = linked_size_.begin(); it != linked_size_.end(); ++ it) {
		tlinked_size& linked_size = it->second;
		
		int at = 0;
		for (std::vector<twidget*>::const_iterator it2 = linked_size.widgets.begin(); it2 != linked_size.widgets.end(); ++ it2, at ++) {
			twidget* widget = *it2;
			tpoint size = widget->layout_size();
			if (dirty) {
				if (linked_size.width) {
					size.x = linked_size.max_size.x;
				}
				if (linked_size.height) {
					size.y = linked_size.max_size.y;
				}
				widget->set_layout_size(size);

			} else {
				if (linked_size.width) {
					VALIDATE(size.x == linked_size.max_size.x, null_str);
				}
				if (linked_size.height) {
					VALIDATE(size.y == linked_size.max_size.y, null_str);
				}
			}
		}
		linked_size.widgets.clear();
	}
}

void tlistbox::init_linked_size_group(const std::string& id, const bool fixed_width, const bool fixed_height)
{
	VALIDATE(fixed_width || fixed_height, null_str);
	VALIDATE(!has_linked_size_group(id), null_str);

	linked_size_[id] = tlinked_size(fixed_width, fixed_height);
}

bool tlistbox::has_linked_size_group(const std::string& id)
{
	return linked_size_.find(id) != linked_size_.end();
}

void tlistbox::add_linked_widget(const std::string& id, twidget& widget, bool linked_group_only)
{
	VALIDATE(!id.empty(), null_str);

	std::map<std::string, tlinked_size>::iterator it = linked_size_.find(id);
	VALIDATE(it != linked_size_.end(), null_str);
	tlinked_size& lg = it->second;

	if (!linked_group_only) {
		VALIDATE(lg.widgets.empty(), null_str);
		return;
	}

	tpoint size = widget.get_best_size();
	bool max_size_changed = false, layout_size_changed = false;
	if (lg.width) {
		if (size.x > lg.max_size.x) {
			lg.max_size.x = size.x;
			max_size_changed = true;

		} else if (size.x < lg.max_size.x) {
			size.x = lg.max_size.x;
			layout_size_changed = true;
		}
	}
	if (lg.height) {
		if (size.y > lg.max_size.y) {
			lg.max_size.y = size.y;
			max_size_changed = true;

		} else if (size.y < lg.max_size.y) {
			size.y = lg.max_size.y;
			layout_size_changed = true;
		}
	}

	if (!linked_max_size_changed_) {
		linked_max_size_changed_ = max_size_changed;
	}

	if (!row_layout_size_changed_) {
		row_layout_size_changed_ = layout_size_changed;
	}

	std::vector<twidget*>& widgets = lg.widgets;
	VALIDATE(std::find(widgets.begin(), widgets.end(), &widget) == widgets.end(), null_str);
	widgets.push_back(&widget);

	widget.set_layout_size(size);
}

void tlistbox::layout_init2()
{
	const int rows = list_grid_->children_vsize();
	VALIDATE(rows && gc_first_at_ != nposm, null_str);
	VALIDATE(gc_calculate_best_size_ || content_->get_width() > 0, null_str);
	VALIDATE(!linked_max_size_changed_, null_str);

	tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	tgrid::tchild* children = list_grid_->children();
	std::set<int> get_best_size_rows;
	{
		tlink_group_owner_lock lock(*this);

		// header
		header->layout_init(true);
		if (row_layout_size_changed_) {
			// set flag. this row require get_best_size again.
			header->set_width(0);
			row_layout_size_changed_ = false;
		}

		// body
		for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
			if (widget->get_visible() != twidget::VISIBLE) {
				continue;
			}

			VALIDATE(!row_layout_size_changed_, null_str);


			// ignore linked_group of toggle_panel
			widget->grid().layout_init(true);
			if (row_layout_size_changed_) {
				// set flag. this row require get_best_size again.
				get_best_size_rows.insert(row);
				row_layout_size_changed_ = false;
			}
		}
		// footer
	}

	layout_linked_widgets(linked_max_size_changed_);

	// maybe result width changed.
	int max_width = content_->get_width();
	int min_height_changed_at = nposm;

	// header
	if (!gc_calculate_best_size_ && (linked_max_size_changed_ || !header->get_width())) {
		tpoint size = header->get_best_size();
		if (size.x > max_width) {
			max_width = size.x;
		}
		header->twidget::set_size(tpoint(0, size.y));
	}

	for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (linked_max_size_changed_ || get_best_size_rows.count(row)) {
			ttoggle_panel& panel = *widget;

			tpoint size = gc_handle_calculate_size(panel, content_->get_width());
			if (min_height_changed_at == nposm && size.y != panel.gc_height_) {
				min_height_changed_at = row;
			}
			
			panel.gc_width_ = size.x;
			panel.gc_height_ = size.y;
			panel.twidget::set_width(0);
		}
		if (widget->gc_width_ > max_width) {
			max_width = widget->gc_width_;
		}
	}

	if (!gc_calculate_best_size_) {
		const bool width_changed = max_width != content_grid_->get_width();
		if (width_changed || !header->get_width()) {
			header->place(header->get_origin(), tpoint(max_width, header->get_height()));
		}

		for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[row].widget_);
			if (widget->get_visible() != twidget::VISIBLE) {
				continue;
			}
			if (width_changed || !widget->get_width()) {
				widget->place(widget->get_origin(), tpoint(max_width, widget->gc_height_));
			}
		}

		if (width_changed) {
			list_grid_->twidget::set_width(max_width);
			content_grid_->twidget::set_width(max_width);

			set_scrollbar_mode(*horizontal_scrollbar_,
				horizontal_scrollbar_mode_,
				content_grid_->get_width(),
				content_->get_width());

			if (horizontal_scrollbar_ != dummy_horizontal_scrollbar_) {
				set_scrollbar_mode(*dummy_horizontal_scrollbar_,
					horizontal_scrollbar_mode_,
					content_grid_->get_width(),
					content_->get_width());
			}
		}
	}

	if (min_height_changed_at != nposm) {
		int next_distance;
		for (int at = min_height_changed_at; at < gc_next_precise_at_; at ++) {
			ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
			VALIDATE(gc_is_precise(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
			if (at != min_height_changed_at) {
				widget->gc_distance_ = next_distance;
				next_distance = widget->gc_distance_ + widget->gc_height_;
			}
			next_distance = widget->gc_distance_ + widget->gc_height_;
		}
	}

	linked_max_size_changed_ = false;
}

int tlistbox::in_which_row(const int x, const int y) const
{
	// only find in [gc_first_at_, gc_last_at_]
	if (gc_first_at_ == nposm) {
		return nposm;
	}
	const tgrid::tchild* children = list_grid_->children();
	for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
		const twidget* widget = children[at].widget_;
		if (point_in_rect(x, y, widget->get_rect())) {
			return at;
		}
	}
	return nposm;
}

bool tlistbox::list_grid_handle_key_up_arrow()
{
	if (cursel_ == nullptr) {
		return false;
	}
	if (!cursel_->at_) {
		return false;
	}
	const tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();

	if (!childs) {
		return false;
	}

	for (int n = cursel_->at_ - 1; n >= 0; -- n) {
		// NOTE we check the first widget to be active since grids have no
		// active flag. This method might not be entirely reliable.
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[n].widget_);
		if (widget->can_selectable()) {
			return select_row(widget->at_);
		}
	}
	return false;
}

// invert: if can not find next, invert=false think find fail immeditaly. invert=true, will find before continue.
ttoggle_panel* tlistbox::next_selectable(const int start_at, const bool invert)
{
	VALIDATE(start_at >= 0, null_str);

	int new_at = nposm;
	const tgrid::tchild* children = list_grid_->children();
	const int size = list_grid_->children_vsize();
	if (size) {
		VALIDATE(start_at <= size, null_str);

		new_at = start_at;
		for (; new_at < size; new_at ++) {
			const tgrid::tchild& child = children[new_at];
			const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(child.widget_);
			if (widget->can_selectable()) {
				break;
			}
		}
		if (new_at == size) {
			if (!invert) {
				return nullptr;
			}
			for (new_at = start_at - 1; new_at >= 0; new_at --) {
				const tgrid::tchild& child = children[new_at];
				const ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(child.widget_);
				if (widget->can_selectable()) {
					break;
				}
			}
			if (new_at < 0) {
				new_at = nposm;
			}
		}
	}
	return new_at != nposm? dynamic_cast<ttoggle_panel*>(children[new_at].widget_): nullptr;
}

bool tlistbox::list_grid_handle_key_down_arrow()
{
	if (cursel_ == NULL) {
		return false;
	}

	tgrid::tchild* children = list_grid_->children();
	const int childs = list_grid_->children_vsize();

	ttoggle_panel* widget = next_selectable(cursel_->at_ + 1, false);
	if (widget) {
		return select_row(widget->at_);
	}
	return false;
}

void tlistbox::handle_key_up_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_up_arrow();

	if (changed) {
		scroll_to_row(cursel_->at_);
	}

	handled = true;
}

void tlistbox::handle_key_down_arrow(SDL_Keymod modifier, bool& handled)
{
	bool changed = list_grid_handle_key_down_arrow();

	if (changed) {
		scroll_to_row(cursel_->at_);
	}

	handled = true;
}

//
// listbox
//
void tlistbox::tgrid3::listbox_init()
{
	VALIDATE(!children_vsize_, "Must no child in listbox!");
	VALIDATE(!rows_ && !cols_, "Must no row and col in listbox!");

	cols_ = 1; // It is toggle_panel widget.
	col_width_.push_back(0);
	col_grow_factor_.push_back(0);
}

int tlistbox::tgrid3::listbox_insert_child(twidget& widget, int at)
{
	// make sure the new child is valid before deferring
	widget.set_parent(this);
	if (at == nposm || at > children_vsize_) {
		at = children_vsize_;
	}

	// below memmove require children_size_ are large than children_vsize_!
	// resize_children require large than exactly rows_*cols_. because children_vsize_ maybe equal it.

	// i think, all grow factor is 0. except last stuff.
	row_grow_factor_.push_back(0);
	rows_ ++;
	row_height_.push_back(0);

	resize_children((rows_ * cols_) + 1);
	if (children_vsize_ - at) {
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	children_vsize_ ++;

	// replacement
	children_[at].flags_ = VERTICAL_ALIGN_TOP | HORIZONTAL_ALIGN_EDGE;

	for (int at2 = at + 1; at2 < children_vsize_; at2 ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at2].widget_);
		widget->at_ ++;
	}

	return at;
}

void tlistbox::tgrid3::listbox_erase_child(int at)
{
	if (at == nposm) {
		at = children_vsize_ - 1;
	}
	if (at < 0 || at >= children_vsize_) {
		return;
	}
	if (children_[at].widget_ == listbox_.cursel_) {
		listbox_.cursel_ = nullptr;
	}
	delete children_[at].widget_;
	children_[at].widget_ = NULL;
	if (at < children_vsize_ - 1) {
		memcpy(&(children_[at]), &(children_[at + 1]), (children_vsize_ - at - 1) * sizeof(tchild));

	}
	children_[children_vsize_ - 1].widget_ = NULL;

	children_vsize_ --;
	rows_ --;
	row_grow_factor_.pop_back();
	row_height_.pop_back();

	for (int at2 = at; at2 < children_vsize_; at2 ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at2].widget_);
		widget->at_ --;
	}
}

void tlistbox::tgrid3::validate_children_continuous() const
{
	if (listbox_.gc_first_at_ == nposm) {
		return;
	}

	VALIDATE(listbox_.gc_first_at_ <= listbox_.gc_last_at_, null_str);

	int next_row_at = 0;
	int next_distance = 0;
	const int cursel_at = listbox_.cursel_? listbox_.cursel_->at_: nposm;
	for (int at = 0; at < children_vsize_; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[at].widget_);
		if (at < listbox_.gc_first_at_ || at > listbox_.gc_last_at_) {
			// rows out of [gc_first_at_, gc_last_at_] must not exist canvas.
			const tgrid& grid = widget->layout_grid();
			VALIDATE(grid.canvas_texture_is_null(), null_str);
		}
		// all row must visible
		VALIDATE(widget->get_visible() == twidget::VISIBLE, null_str);

		// distance
		if (at < listbox_.gc_next_precise_at_) {
			VALIDATE(gc_is_precise(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
			VALIDATE(widget->gc_distance_ == next_distance, null_str);
			next_distance += widget->gc_height_;

		} else if (at == listbox_.gc_next_precise_at_) {
			VALIDATE(widget->gc_distance_ == nposm && widget->gc_height_ == nposm, null_str);

		} else if (at >= listbox_.gc_first_at_ && at <= listbox_.gc_last_at_) {
			if (at == listbox_.gc_first_at_) {
				VALIDATE(gc_is_estimated(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
				next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;

			} else {
				VALIDATE(gc_is_estimated(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
				VALIDATE(gc_distance_value(widget->gc_distance_) == next_distance, null_str);
				next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;
			}

		} else {
			// if gc_distance != nposm, it's gc_height_ must valid. but if gc_height_ is valid, gc_distance_ maybe npos.
			VALIDATE(widget->gc_distance_ == nposm || widget->gc_height_ != nposm, null_str);
		}

		// at_ must be continues.
		VALIDATE(next_row_at == widget->at_, null_str);
		next_row_at ++;

		// support must one selectable.
		if (widget->get_value()) {
			VALIDATE(widget->at_ == cursel_at, null_str);
		} else {
			VALIDATE(widget->at_ != cursel_at, null_str);
		}
	}
}

void tlistbox::tgrid3::layout_init(bool linked_group_only)
{
	// listbox doesn't use normal layout_init flow.
	VALIDATE(false, null_str);
}

tpoint tlistbox::tgrid3::calculate_best_size() const
{
	VALIDATE(cols_ == 1, null_str);
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	if (!children_vsize_) {
		return tpoint(0, 0);
	}

	if (listbox_.gc_first_at_ == nposm) {
		tgc_calculate_best_size_lock lock(listbox_);
		listbox_.mini_handle_gc(0, 0);
		VALIDATE(listbox_.gc_first_at_ != nposm, null_str);
	}

	int max_width = 0;
	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);

		if (widget->gc_width_ > max_width) {
			max_width = widget->gc_width_;
		}

		VALIDATE(widget->gc_height_ != nposm, null_str);
		row_height_[row] = widget->gc_height_;
	}
	col_width_[0] = max_width;

	return tpoint(col_width_[0], listbox_.gc_calculate_total_height());
}

tpoint tlistbox::tgrid3::calculate_best_size_bh(const int width)
{
	return tpoint(nposm, nposm);
}

void tlistbox::tgrid3::place(const tpoint& origin, const tpoint& size)
{
	twidget::place(origin, size);
	if (!children_vsize_) {
		return;
	}

	VALIDATE(listbox_.gc_first_at_ != nposm, null_str);
	VALIDATE(listbox_.gc_current_content_width_ != nposm, null_str);

	VALIDATE(cols_ == 1, null_str);
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	tpoint orig = origin;
	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; ++ row) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);

		orig.y = origin.y + gc_distance_value(widget->gc_distance_);
		widget->place(orig, tpoint(size.x, row_height_[row]));
	}
}

void tlistbox::tgrid3::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	if (!children_vsize_) {
		return;
	}
	static int times = 0;
	VALIDATE(listbox_.gc_first_at_ != nposm, null_str);

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);
		VALIDATE(widget, null_str);
/*
				SDL_Rect content = listbox_.content_spacer()->get_rect();
				SDL_Rect content_grid = listbox_.content_grid()->get_rect();
				SDL_Log("[%i]set_origin[#%i] conteng_grid(%i, %i, %i, %i), content(%i, %i, %i, %i), total_height_gc(%i)\n",
					times, row,
					content_grid.x, content_grid.y, content_grid.w, content_grid.h,
					content.x, content.y, content.w, content.h, listbox_.gc_calculate_total_height());
				SDL_Log("[%i]set_origin[#%i] vertical scrollbar, item_position(%i), item_count(%i), visible_items(%i)\n",
					times, row,
					listbox_.vertical_scrollbar_->get_item_position(), listbox_.vertical_scrollbar_->get_item_count(), listbox_.vertical_scrollbar_->get_visible_items());
				SDL_Log("[%i]set_origin[#%i] tgrid3::set_origin, origin(%i, %i)(y2: %i, distance: %i), height: %i, content(%i, %i)\n", 
					times, row,
					origin.x, origin.y, origin.y + gc_distance_value(widget->gc_distance_),
					widget->gc_distance_, widget->gc_height_,
					content.x, content.y);
*/
		// distance  
		//    0    ---> origin.y
		//    1    ---> origin.y + 1
		widget->set_origin(tpoint(origin.x, origin.y + gc_distance_value(widget->gc_distance_)));
	}
	times ++;
}

void tlistbox::tgrid3::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);

		widget->set_visible_area(area);
	}
}

void tlistbox::tgrid3::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);
	clear_dirty();

	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children_[row].widget_);

		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		if (widget->get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		widget->draw_background(frame_buffer, x_offset, y_offset);
		widget->draw_children(frame_buffer, x_offset, y_offset);
		widget->clear_dirty();

		if (listbox_.left_drag_grid_ && listbox_.drag_at_ == row) {
			// if draw whole window, require below statement draw left_drag_grid.
			VALIDATE(listbox_.left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);
			listbox_.left_drag_grid_->draw_background(frame_buffer, x_offset, y_offset);
			listbox_.left_drag_grid_->draw_children(frame_buffer, x_offset, y_offset);

			VALIDATE(listbox_.left_drag_spacer_->get_width() == 0, null_str);
		}
	}
}

void tlistbox::tgrid3::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	
	if (!children_vsize_) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		const tchild& child = children_[row];

		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		child.widget_->dirty_under_rect(clip);
	}
}

twidget* tlistbox::tgrid3::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}
	
	if (!children_vsize_) {
		return nullptr;
	}

	if (listbox_.gc_first_at_ == nposm) {
		return nullptr;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		const tchild& child = children_[row];

		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		twidget* widget = child.widget_->find_at(coordinate, must_be_active);
		if (widget) {
			return widget;
		}
	}
	return nullptr;
}

void tlistbox::tgrid3::layout_children()
{
	if (!children_vsize_) {
		return;
	}
	if (listbox_.gc_first_at_ == nposm) {
		return;
	}

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		tchild& child = children_[row];

		child.widget_->layout_children();
	}
}

void tlistbox::tgrid3::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (!children_vsize_) {
		return;
	}

	twindow* window = get_window();
	std::vector<std::vector<twidget*> >& dirty_list = window->dirty_list();

	VALIDATE(!call_stack.empty() && call_stack.back() == this, null_str);

	for (int row = listbox_.gc_first_at_; row <= listbox_.gc_last_at_; row ++) {
		tchild& child = children_[row];

		std::vector<twidget*> child_call_stack = call_stack;
		child.widget_->populate_dirty_list(caller, child_call_stack);
	}
}

namespace {

/**
 * Swaps an item in content_grid with widget.*/
void swap_grid(tgrid& content_grid, twidget& widget, const std::string& id)
{
	// Make sure the new child has same id.
	widget.set_id(id);

	// Get the container containing the wanted widget.
	tgrid* parent_grid = find_widget<tgrid>(&content_grid, id, true, false);
	parent_grid = dynamic_cast<tgrid*>(parent_grid->parent());
	VALIDATE(parent_grid, null_str);

	// Replace the child.
	twidget* original_widget = parent_grid->swap_child(id, &widget, false);
	VALIDATE(original_widget, null_str);

	delete original_widget;
}

} // namespace


void tlistbox::finalize(tbuilder_grid_const_ptr header, tbuilder_grid_const_ptr footer)
{
	if (header) {
		swap_grid(*content_grid_, *header->build(), "_header_grid");
		// now, header always in top, require fixed in future.
		unrestricted_drag_ = false;
	}

	if (footer) {
		swap_grid(*content_grid_, *footer->build(), "_footer_grid");
	}

	list_grid_ = new tgrid3(*this);
	list_grid_->listbox_init();
	swap_grid(*content_grid_, *list_grid_, "_list_grid");
}

void tlistbox::mini_more_items()
{
	if (did_more_rows_gc_) {
		did_more_rows_gc_(*this);
	}
}

void tlistbox::reset()
{
	tlink_group_owner_lock lock(*this);

	// if content width change, require reset.
	tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	header->layout_init(false);

	gc_first_at_ = gc_last_at_ = nposm;
	gc_next_precise_at_ = 0;
	tgrid::tchild* children = list_grid_->children();
	const int rows = list_grid_->children_vsize();
	for (int at = 0; at < rows; at ++) {
		ttoggle_panel* widget = dynamic_cast<ttoggle_panel*>(children[at].widget_);
		widget->gc_distance_ = widget->gc_height_ = widget->gc_width_ = nposm;
		widget->layout_init(false);
	}

	for (std::map<std::string, tlinked_size>::iterator it = linked_size_.begin(); it != linked_size_.end(); ++ it) {
		tlinked_size& linked_size = it->second;
		linked_size.max_size.x = linked_size.max_size.y = 0;
	}
}

tpoint tlistbox::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	const int rows = list_grid_->children_vsize();
	if (rows) {
		if (left_drag_grid_ && drag_at_ != nposm) {
			// hope don't enter it.
			drag_end();
		}
		if (longpress_at_ != nposm) {
			longpress_end();
		}

		// once modify content rect, must call it.
		if (gc_first_at_ != nposm && gc_current_content_width_ != content_size.x) {
			reset();
		}
		if (gc_first_at_ == nposm) {
			mini_handle_gc(0, 0);
		}
	}
	return content_grid_->get_best_size();
}

void tlistbox::drag_motioned(const int offset)
{
	VALIDATE(drag_judge_at_ == nposm && drag_at_ != nposm && offset >= 0, null_str);
	VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

	const tgrid::tchild* children = list_grid_->children();
	twidget* widget = children[drag_at_].widget_;
	widget->set_origin(tpoint(content_grid_->get_x() - offset, widget->get_y()));
	SDL_Rect area = ::create_rect(content_->get_x(), widget->get_y(), content_->get_width() - drag_offset_, widget->get_height());

	SDL_Rect clip;
	const SDL_Rect content_rect = content_->get_rect();
	SDL_IntersectRect(&area, &content_rect, &clip);
	widget->set_visible_area(clip);

	area = ::create_rect(left_drag_grid_->get_x() + left_drag_grid_size_.x - offset, left_drag_grid_->get_y(), 
		offset, left_drag_grid_->get_height());
	if (offset > left_drag_grid_size_.x) {
		area.x = left_drag_grid_->get_x();
		area.w = left_drag_grid_size_.x;
	}

	SDL_IntersectRect(&area, &content_rect, &clip);
	left_drag_grid_->set_visible_area(clip);

	area.x = area.y = area.w = area.h = 0;
	if (offset > left_drag_grid_size_.x) {
		area.x = content_->get_x() + content_->get_width() - offset;
		area.y = widget->get_y();
		area.w = offset - left_drag_grid_size_.x;
		area.h = widget->get_height();
	}
	if (area != left_drag_spacer_->get_rect()) {
		bool intersect = SDL_IntersectRect(&area, &content_rect, &clip);
		if (!intersect) {
            if (clip.w < 0) {
                clip.w = 0;
            }
            if (clip.h < 0) {
                clip.h = 0;
            }
        }
		left_drag_spacer_->place(tpoint(clip.x, clip.y), tpoint(clip.w, clip.h));
	}
}

void tlistbox::drag_end()
{
	VALIDATE(drag_judge_at_ == nposm && drag_at_ >= 0 && drag_at_ < list_grid_->children_vsize(), null_str);
	VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

	{
		tdisable_invalidate_layout_lock lock;
		left_drag_grid_->set_visible(twidget::INVISIBLE);
		left_drag_spacer_->set_visible(twidget::INVISIBLE);
	}

	const tgrid::tchild* children = list_grid_->children();
	twidget* widget = children[drag_at_].widget_;
	widget->set_origin(tpoint(content_grid_->get_x(), widget->get_y()));
	SDL_Rect area = ::create_rect(content_->get_x(), widget->get_y(), content_->get_width(), widget->get_height());

	SDL_Rect clip;
	const SDL_Rect content_rect = content_->get_rect();
	SDL_IntersectRect(&area, &content_rect, &clip);
	widget->set_visible_area(clip);

	out_of_chain_widget_lock_.reset(nullptr);

	drag_at_ = nposm;
	drag_offset_ = 0;
}

void tlistbox::click_left_drag_widget(const std::string& id)
{
	VALIDATE(drag_at_ != nposm, null_str);

	std::map<std::string, boost::function<void (tlistbox&, ttoggle_panel&)> >::const_iterator it = left_drag_widget_click_.find(id);
	VALIDATE(it != left_drag_widget_click_.end(), null_str);

	ttoggle_panel& row = row_panel(drag_at_);
	it->second(*this, row);
	// it->second maybe execute erase row/clear, it will result drag_at_ to nposm.
	cancel_drag();
}

tbutton& tlistbox::set_did_left_drag_widget_click(const std::string& id, const boost::function<void (tlistbox&, ttoggle_panel&)>& did)
{
	VALIDATE(left_drag_grid_ && !id.empty(), null_str);
	tbutton* button = dynamic_cast<tbutton*>(left_drag_grid_->find(id, true));
	VALIDATE(button, null_str);

	std::map<std::string, boost::function<void (tlistbox&, ttoggle_panel&)> >::iterator it = left_drag_widget_click_.find(id);
	if (it != left_drag_widget_click_.end()) {
		left_drag_widget_click_.erase(it);
	} else {
		connect_signal_mouse_left_click(
			*button
			, boost::bind(
				&tlistbox::click_left_drag_widget
				, this
				, id));
	}
	left_drag_widget_click_.insert(std::make_pair(id, did));

	return *button;
}

void tlistbox::longpress_show_menu(const bool show)
{
	VALIDATE(longpress_at_ != nposm, null_str);
	VALIDATE(did_longpress_memu_, null_str);

	longpress_menu_.clear();
	did_longpress_memu_(*this, row_panel(longpress_at_), longpress_menu_);

	twindow* window = get_window();
	if (show) {
		int total_width = 0;
		
		std::vector<int> best_widths;
		for (std::vector<tfloat_widget*>::const_iterator it = longpress_menu_.begin(); it != longpress_menu_.end(); ++ it) {
			const tfloat_widget& item = **it;
			best_widths.push_back(item.widget->get_best_size().x);
			total_width += best_widths.back();
		}

		const int min_edge_gap = 4 * twidget::hdpi_scale;
		VALIDATE((int)window->get_width() >= min_edge_gap + total_width, null_str);

		int x, y;
		SDL_GetMouseState(&x, &y);
		if (x < min_edge_gap + total_width) {
			x = min_edge_gap + total_width;
		} else if (x + total_width + min_edge_gap > (int)window->get_width()) {
			x = window->get_width() - total_width - min_edge_gap;
		}
		
		int at = 0;
		for (std::vector<tfloat_widget*>::const_iterator it = longpress_menu_.begin(); it != longpress_menu_.end(); ++ it, at ++) {
			tfloat_widget& item = **it;
			item.set_ref_widget(this, tpoint(x, y));
			item.set_visible(true);
			x += best_widths[at];
		}

	} else {
		for (std::vector<tfloat_widget*>::const_iterator it = longpress_menu_.begin(); it != longpress_menu_.end(); ++ it) {
			tfloat_widget& item = **it;
			item.set_visible(false);
		}
	}
}

void tlistbox::longpress_timer_handler()
{
	if (drag_judge_at_ == nposm || longpress_at_ != nposm) {
		return;
	}

	twindow* window = get_window();
	const twidget* current_mouse_focus = window->mouse_captured_widget();
	if (current_mouse_focus && !current_mouse_focus->parent()->is_scroll_container()) {
		if (!current_mouse_focus->captured_mouse_can_to_scroll_container()) {
			drag_judge_at_ = nposm;
			return;
		}
	}

	uint32_t now = SDL_GetTicks();
	const int longpress_threshold = 1000;
	if (now >= longpress_ticks_ + longpress_threshold) {
		longpress_at_ = drag_judge_at_;
		last_longpress_at_ = drag_judge_at_;
		drag_judge_at_ = nposm;

		refuse_scroll_lock_.reset(new trefuse_scroll_lock);
		out_of_chain_widget_lock_.reset(new tout_of_chain_widget_lock(*this));

		longpress_ticks_ = 0;
		longpress_show_menu(true);
	}
}

void tlistbox::longpress_end()
{
	VALIDATE(drag_judge_at_ == nposm && longpress_at_ >= 0 && longpress_at_ < list_grid_->children_vsize(), null_str);
	longpress_show_menu(false);

	out_of_chain_widget_lock_.reset(nullptr);
	longpress_at_ = nposm;
}

bool tlistbox::now_can_drag() const
{
	return left_drag_grid_ && horizontal_scrollbar_->get_item_position() + horizontal_scrollbar_->get_visible_items() == horizontal_scrollbar_->get_item_count();
}

void tlistbox::mini_mouse_down(const tpoint& first)
{
	VALIDATE(drag_judge_at_ == nposm, null_str);
	VALIDATE(!refuse_scroll_lock_.get(), null_str);
	twindow* window = get_window();

	if (drag_at_ == nposm && longpress_at_ == nposm) {
		VALIDATE(!drag_offset_, null_str);
		// 1. drag, 2. longpress
		if (use_drag_) {
			if (!now_can_drag()) {
				return;
			}
		} else {
			if (!did_longpress_memu_) {
				return;
			}
			VALIDATE(!longpress_timer_.valid(), null_str);
			longpress_ticks_ = SDL_GetTicks();
			longpress_timer_.reset(100, *window, boost::bind(&tlistbox::longpress_timer_handler, this));
		}
		drag_judge_at_ = in_which_row(first.x, first.y);
		if (drag_judge_at_ == nposm) {
			// content_grid maybe less than content, mouse maybe press at bonus part.
			return;
		}
		if (use_drag_) {
			if (did_can_drag_ && !did_can_drag_(*this, row_panel(drag_judge_at_))) {
				drag_judge_at_ = nposm;

			}
		}
		

	} else if (use_drag_) {
		drag_end();
		// end drag will consume this down.
		window->clear_mouse_click();

	} else {
		longpress_end();
		// end longpress will consume this down.
		window->clear_mouse_click();
	}
}

bool tlistbox::mini_mouse_motion(const tpoint& first, const tpoint& last)
{ 
	if (drag_judge_at_ == nposm && drag_at_ == nposm && longpress_at_ == nposm) {
		return true;
	}
	if (drag_judge_at_ != nposm) {
		// judging...
		if (last.x > first.x) {
			// must not move to right.
			drag_judge_at_ = nposm;
			return true;

		}
		const tgrid::tchild* children = list_grid_->children();
		twidget* widget = children[drag_judge_at_].widget_;
		if (!point_in_rect(last.x, last.y, widget->get_rect())) {
			// moved to other row.
			drag_judge_at_ = nposm;
			return true;
		}
		const int abs_diff_x = abs(last.x - first.x);
		const int abs_diff_y = abs(last.y - first.y);
		if (abs_diff_y > 2 * settings::clear_click_threshold) {
			// vertical move is large then one threshold. of couse, it maybe not relative with clear_click_threshold
			drag_judge_at_ = nposm;
			return true;
		}

		if (use_drag_) {
			if (first.x - last.x >= settings::clear_click_threshold) {
				drag_at_ = drag_judge_at_;
				drag_judge_at_ = nposm;

				refuse_scroll_lock_.reset(new trefuse_scroll_lock);
				out_of_chain_widget_lock_.reset(new tout_of_chain_widget_lock(*this));
				VALIDATE(left_drag_grid_->get_visible() == twidget::INVISIBLE, null_str);
				// app's drag_started_ maybe modify grid's content, requrie calcualte size again.
				left_drag_grid_size_ = left_drag_grid_->get_best_size();
				VALIDATE(left_drag_grid_size_.y <= (int)widget->get_height(), "height of left drag menu must <= height of row.");

				if (selectable_ && cursel_ && cursel_->at_ != drag_at_) {
					select_row(drag_at_);
				}

				left_drag_grid_->place(tpoint(content_->get_x() + content_->get_width() - left_drag_grid_size_.x, widget->get_y()),
					tpoint(left_drag_grid_size_.x, widget->get_height()));
				{
					tdisable_invalidate_layout_lock lock;
					left_drag_grid_->set_visible(twidget::VISIBLE);
					left_drag_spacer_->set_visible(twidget::VISIBLE);
				}

				drag_offset_ = abs_diff_x;
				drag_motioned(drag_offset_);
			}

		} else {
			if (abs_diff_x >= settings::clear_click_threshold || abs_diff_x >= settings::clear_click_threshold) {
				// has moved.
				longpress_ticks_ = 0;
				drag_judge_at_ = nposm;
				return true;
			}

		}
		return false;

	}

	if (longpress_at_ != nposm) {
		// once longpressed, refuse move continue.
		return false;
	}

	if (drag_at_ != nposm) {
		// draging
		VALIDATE(drag_judge_at_ == nposm, null_str);
		VALIDATE(left_drag_grid_->get_visible() == twidget::VISIBLE, null_str);

		int diff = first.x - last.x;
		if (diff > left_drag_grid_size_.x) {
			// diff = left_drag_grid_size_.x;
		} else if (diff < 0) {
			diff = 0;
		}

		drag_offset_ = diff;
		drag_motioned(drag_offset_);
	}
	return false;
}

void tlistbox::mini_mouse_leave(const tpoint& first, const tpoint& last)
{
	drag_judge_at_ = nposm;
	refuse_scroll_lock_.reset(nullptr);

	if (use_drag_) {
		VALIDATE(longpress_at_ == nposm, null_str);
		if (drag_at_ != nposm) {
			const int all_threshold = left_drag_grid_size_.x / 3;
			bool require_drag_end = true;
			if (drag_offset_ >= all_threshold) {
				drag_offset_ = left_drag_grid_->get_width();
				drag_motioned(drag_offset_);

				if (left_drag_grid_->has_child_visible()) {
					require_drag_end = false;
				}

			}
			if (require_drag_end) {
				drag_end();
			}
		}

	} else {
		VALIDATE(drag_at_ == nposm, null_str);
		longpress_timer_.reset();
	}
}

bool tlistbox::mini_wheel()
{
	if (!is_null_coordinate(first_coordinate_) && drag_at_ != nposm) {
		return false;
	}

	if (drag_at_ != nposm) {
		drag_end();
	}
	if (longpress_at_ != nposm) {
		longpress_end();
	}

	return true;
}

void tlistbox::signal_handler_out_of_chain(const int e, twidget* extra_widget)
{
	if (drag_at_ != nposm) {
		drag_end();
	}
	if (longpress_at_ != nposm) {
		bool require_end = true;
		if (e == event::LEFT_BUTTON_DOWN || e == event::MIDDLE_BUTTON_DOWN || e == event::RIGHT_BUTTON_DOWN) {
			for (std::vector<tfloat_widget*>::const_iterator it = longpress_menu_.begin(); it != longpress_menu_.end(); ++ it) {
				const twidget* widget = (*it)->widget.get();
				if (widget == extra_widget) {
					require_end = false;
					break;
				}
			}
		}
		if (require_end) {
			longpress_end();
		}
	}
}

void tlistbox::set_list_builder(tbuilder_grid_ptr list_builder)
{ 
	VALIDATE(!list_builder_.get(), null_str);

	list_builder_ = list_builder;
	VALIDATE(list_builder_.get(), null_str);

	size_t s = list_builder_->widgets.size();

	if (list_builder_->widgets.size() >= 2) {
		left_drag_grid_ = dynamic_cast<tgrid*>(list_builder->widgets[1]->build());
		left_drag_grid_->set_visible(twidget::INVISIBLE);

		// don't set row(toggle_panel) to it's parent.
		// grid mayb containt "erase" button, it will erase row(toggle_panel). if it is parent, thine became complex.
		left_drag_grid_->set_parent(this);

		left_drag_spacer_ = new tspacer();
		left_drag_spacer_->set_definition("default", null_str);
		left_drag_spacer_->set_visible(twidget::INVISIBLE);
		left_drag_spacer_->set_parent(this);

	} else {
		use_drag_ = false;
	}
}

const std::string& tlistbox::get_control_type() const
{
	static const std::string type = "listbox";
	return type;
}

} // namespace gui2
