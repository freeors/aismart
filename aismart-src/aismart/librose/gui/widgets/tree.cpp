/* $Id: tree_view.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2010 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/tree.hpp"

#include "gui/auxiliary/widget_definition/tree.hpp"
#include "gui/auxiliary/window_builder/tree.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/tree_node.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/spacer.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(tree)

ttree::ttree(const std::vector<tnode_definition>& node_definitions, const bool foldable)
	: tscroll_container(2)
	, grid2_(nullptr)
	, node_definitions_(node_definitions)
	, indention_step_size_(0)
	, root_node_(new ttree_node(
		  "root"
		, node_definitions_
		, NULL
		, *this
		, std::map<std::string, std::string>()
		, false))
	, foldable_(foldable)
	, selectable_(true)
	, cursel_(nullptr)
	, gc_nodes_(std::make_pair(0, 0))
	, gc_lookup_(nullptr)
	, gc_lookup_size_(0)
	, gc_insert_cache_(nullptr)
	, gc_insert_cache_size_(0)
	, gc_insert_cache_vsize_(0)
	, gc_insert_or_erase_(false)
	, gc_insert_first_at_lock_(false)
	, left_align_(false)
	, no_indentation_(false)
	, linked_max_size_changed_(false)
	, row_layout_size_changed_(false)
{
	connect_signal<event::LEFT_BUTTON_DOWN>(
			  boost::bind(
				    &ttree::signal_handler_left_button_down
				  , this
				  , _2)
			, event::tdispatcher::back_pre_child);
}

ttree::~ttree()
{
	if (gc_insert_cache_) {
		free(gc_insert_cache_);
	}
	if (gc_lookup_) {
		free(gc_lookup_);
	}
}

void ttree::enable_select(const bool enable)
{
	if (enable == selectable_) {
		return;
	}
	selectable_ = enable;
	if (enable) {
		VALIDATE(!cursel_, null_str);
	} else {
		select_node(nullptr);
	}
}

ttree_node& ttree::insert_node(const std::string& id, const std::map<std::string, std::string>& data, int at, const bool branch)
{
	return get_root_node().insert_node(id, data, at, branch);
}

void ttree::erase_node(ttree_node& node)
{
	erase_node_internal(node, false);
}

void ttree::erase_node_internal(ttree_node& node, const bool erase_children)
{
	VALIDATE(node.parent_node_, null_str);

	if (node.gc_is_shown()) {
		gc_lookup_erase(node, false, false);
	}

	const int from_at = node.at_;
	ttree_node* parent_node = node.parent_node_;

	std::vector<ttree_node*>::iterator it = std::find(node.parent_node_->children_.begin(), node.parent_node_->children_.end(), &node);
	VALIDATE(it != node.parent_node_->children_.end(), null_str);

	delete(*it);
	if (!erase_children) {
		node.parent_node_->children_.erase(it);

		it = parent_node->children_.begin();
		std::advance(it, from_at);
		for (; it != parent_node->children_.end(); ++ it) {
			ttree_node& node = **it;
			node.at_ --;
		}
		validate_children_continuous();
	}

	invalidate_layout(nullptr);
}

void ttree::scroll_to_node(const ttree_node& node)
{
	if (gc_current_content_width_ <= 0) {
		// listbox not ready to gc.
		return;
	}

	if (!gc_nodes_.second) {
		return;
	}

	const ttree_node& node2 = node.gc_serie_first_shown();
	gc_locked_at_ = gc_lookup_find(&node2);
	mini_handle_gc(horizontal_scrollbar_->get_item_position(), vertical_scrollbar_->get_item_position());
}


#define panel_from_children(at)		children[at]

tpoint ttree::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	tpoint size = content_size;
	if (left_align_ && !empty()) {
		ttoggle_panel* item = (*root_node_->children_.begin());
		// by this time, hasn't called place(), cannot use get_size().
		int height = item->get_best_size().y;
		if (height <= size.y) {
			int list_height = size.y / height * height;

			// reduce hight if allow height > get_best_size().y
			height = root_node_->get_best_size().y;
			if (list_height > height) {
				list_height = height;
			}
			size.y = list_height;
			if (size.y != content_size.y) {
				content_->set_size(size);
			}
		}
	}

	const int rows = gc_nodes_.second;
	if (rows) {
		// once modify content rect, must call it.
		if (gc_first_at_ != nposm && gc_current_content_width_ != content_size.x) {
			tlink_group_owner_lock lock(*this);

			gc_first_at_ = gc_last_at_ = nposm;
			gc_next_precise_at_ = 0;
			ttree_node** children = gc_lookup_;
			for (int at = 0; at < rows; at ++) {
				ttoggle_panel* widget = panel_from_children(at);
				widget->gc_distance_ = widget->gc_height_ = widget->gc_width_ = nposm;
				widget->layout_init(false);
			}
/*
			for (std::map<std::string, tlinked_size>::iterator it = linked_size_.begin(); it != linked_size_.end(); ++ it) {
				tlinked_size& linked_size = it->second;
				linked_size.max_size.x = linked_size.max_size.y = 0;
			}
*/
		}
		if (gc_first_at_ == nposm) {
			mini_handle_gc(0, 0);
		}
	}

	return content_grid_->get_best_size();
}

bool ttree::empty() const
{
	return root_node_->empty();
}

void ttree::select_node(ttree_node* node)
{
	if (node) {
		node = &node->gc_serie_first_shown();
	}

	if (cursel_ == node) {
		return;
	}

	// Deselect current item
	if (cursel_) {
		cursel_->set_value(false);
	}

	if (selectable_) {
		cursel_ = node;
		cursel_->set_value(true);
	}

	if (did_node_changed_) {
		did_node_changed_(*this, *node);
	}
}

tpoint ttree::adjust_content_size(const tpoint& size)
{
	if (!left_align_ || empty()) {
		return size;
	}
	ttoggle_panel* item = (*root_node_->children_.begin());
	// by this time, hasn't called place(), cannot use get_size().
	int height = item->get_best_size().y;
	if (height > size.y) {
		return size;
	}
	int list_height = size.y / height * height;

	// reduce hight if necessary.
	height = root_node_->get_best_size().y;
	if (list_height > height) {
		list_height = height;
	}
	return tpoint(size.x, list_height);
}

void ttree::adjust_offset(int& x_offset, int& y_offset)
{
	if (!left_align_ || empty() || !y_offset) {
		return;
	}
	ttoggle_panel* item = (*root_node_->children_.begin());
	int height = item->get_size().y;
	if (y_offset % height) {
		y_offset = y_offset / height * height + height;
	}
}

void ttree::show_row_rect(const int at)
{
	ttree_node** children = gc_lookup_;
	const int rows = gc_nodes_.second;

	VALIDATE(at >=0 && at < rows && gc_first_at_ != nposm, null_str);

	const ttoggle_panel* first_widget = panel_from_children(gc_first_at_);
	const ttoggle_panel* widget = panel_from_children(at);
	SDL_Rect rect{0, gc_distance_value(widget->gc_distance_), 0, widget->gc_height_};
	
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

void ttree::garbage_collection(ttoggle_panel& row)
{
	row.clear_texture();
}

tpoint ttree::gc_handle_calculate_size(ttoggle_panel& widget, const int width) const
{
	ttree_node* node = dynamic_cast<ttree_node*>(&widget);
	return node->get_best_size();
}

ttoggle_panel* ttree::gc_widget_from_children(const int at) const
{
	return gc_lookup_[at];
}

int ttree::gc_handle_update_height(const int new_height)
{
	SDL_Rect rect = content_grid_->get_rect();
	const int diff = new_height - rect.h;
	content_grid_->twidget::place(tpoint(rect.x, rect.y), tpoint(rect.w, rect.h + diff));

	return diff;
}

void ttree::validate_children_continuous() const
{
	if (gc_first_at_ == nposm) {
		return;
	}

	VALIDATE(gc_first_at_ <= gc_last_at_, null_str);

	int next_row_at = 0;
	int next_distance = 0;
	root_node_->validate_children_continuous(next_row_at, next_distance, false);
}

void ttree::invalidate_layout(twidget* widget)
{
	if (widget) {
		VALIDATE(!widget->float_widget(), null_str);
		// if widget is row(toggle_panel), needn't execute in below if(tmp), so use parent at first.
		twidget* tmp = widget->parent();
		while (tmp) {
			if (tmp->get_control_type() == "toggle_panel" && tmp->parent()->id() == "root") {
				break;
			}
			tmp = tmp->parent();
		}
		if (tmp) {
			ttree_node& node = *dynamic_cast<ttree_node*>(tmp);
			const int at = node.at();
			// now ttree_node not suport layout_init2 dynamic, require fix bug in future!
			if (gc_first_at_ != nposm && at >= gc_first_at_ && at <= gc_last_at_) {
				{
					tlink_group_owner_lock lock(*this);
					node.layout_init(false);
				}
				layout_init2();
				scrollbar_moved(true);
			}
		}
	}
	tscroll_container::invalidate_layout(widget);
}

void ttree::gc_insert_cache_resize(int size)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > gc_insert_cache_size_) {
		ttree_node** tmp = (ttree_node**)malloc(size);
		if (gc_insert_cache_) {
			if (gc_insert_cache_vsize_) {
				memcpy(tmp, gc_insert_cache_, gc_insert_cache_vsize_);
			}
			free(gc_insert_cache_);
			gc_insert_cache_ = nullptr;
		}
		gc_insert_cache_ = tmp;
		gc_insert_cache_size_ = size;
	}
}

void ttree::gc_lookup_resize(int size, const int vsize)
{
	size = posix_align_ceil(size, 4096);
	VALIDATE(size >= 0, null_str);

	if (size > gc_lookup_size_) {
		ttree_node** tmp = (ttree_node**)malloc(size);
		if (gc_lookup_) {
			if (vsize) {
				memcpy(tmp, gc_lookup_, vsize);
			}
			free(gc_lookup_);
			gc_lookup_ = nullptr;
		}
		gc_lookup_ = tmp;
		gc_lookup_size_ = size;
	}
}

int ttree::gc_lookup_find(const ttree_node* node) const
{
	VALIDATE(node, null_str);

	int hit_at = nposm;
	int start = 0;
	int end = gc_nodes_.second - 1; // 1 is new, 1 is count - 1.
	int mid = (end - start) / 2 + start;
	ttree_node* current_gc_row = nullptr;
	while (mid - start > 1) {
		current_gc_row = gc_lookup_[mid];
		if (node == current_gc_row) {
			hit_at = mid;
			break;
		}
		if (node->is_front(*current_gc_row)) {
			end = mid;

		} else {
			start = mid;

		}
		mid = (end - start) / 2 + start;
	}
	
	if (hit_at == nposm) {
		int row = start;
		for (; row <= end; row ++) {
			current_gc_row = gc_lookup_[row];
			if (node == current_gc_row) {
				hit_at = row;
				break;
			}
		}
		VALIDATE(hit_at != nposm, null_str);
	}
	return hit_at;
}

void ttree::gc_lookup_insert(ttree_node& node, const ttree_node* back_node, const bool only_children)
{
	int insert_at;
	if (!only_children) {
		insert_at = back_node? gc_lookup_find(back_node) + 1: 0;
	} else {
		// if only_children, node is hit_node, back_node must null.
		VALIDATE(!back_node, null_str);
		insert_at = gc_lookup_find(&node) + 1;
	}
	
	{
		tcalculate_gc_insert_cache_vsize_lock lock(*this, true);
		node.calculate_gc_insert_cache_vsize(only_children, false);

		const int count = gc_insert_cache_vsize_ / sizeof(ttree_node*);
		gc_lookup_resize((gc_nodes_.second + count) * sizeof(ttree_node*), gc_nodes_.second * sizeof(ttree_node*));
		gc_nodes_.second += count;
		if (insert_at < gc_nodes_.second - count) {
			memmove(&(gc_lookup_[insert_at + count]), &(gc_lookup_[insert_at]), (gc_nodes_.second - insert_at - count) * sizeof(ttree_node*));
		}

		memcpy(&gc_lookup_[insert_at], gc_insert_cache_, gc_insert_cache_vsize_);
		gc_insert_or_erase_row(insert_at, true, gc_insert_cache_, count);
	}
}

void ttree::gc_lookup_erase(ttree_node& node, const bool only_children, const bool must_include_children)
{
	const int erase_at = gc_lookup_find(&node) + (only_children? 1: 0);

	{
		tcalculate_gc_insert_cache_vsize_lock lock(*this, false);
		node.calculate_gc_insert_cache_vsize(only_children, must_include_children);
		const int count = gc_insert_cache_vsize_ / sizeof(ttree_node*);

		if (erase_at < gc_nodes_.second - count) {
			memcpy(&(gc_lookup_[erase_at]), &(gc_lookup_[erase_at + count]), (gc_nodes_.second - erase_at - count) * sizeof(ttree_node*));
		}
		gc_nodes_.second -= count;
		gc_insert_or_erase_row(erase_at, false, gc_insert_cache_, count);
	}
}

void ttree::gc_lookup_reset()
{
	gc_nodes_.second = 0;
}

void ttree::gc_insert_or_erase_row(const int row, const bool insert, ttree_node** nodes, const int count)
{
	// at this time, gc_lookup hasn't insert or erase new nodes.
	// why not wait inserted or erased? because of gc_first_at_ and gc_last_at_.

	VALIDATE(count >= 1, null_str);
	const int rows = gc_nodes_.second;
	ttree_node** children = gc_lookup_;

	//
	// session1: gc_next_precise_at_
	//
	// 2)adjust gc_next_precies_at_.
	if (row <= gc_next_precise_at_) {
		// some row height changed in precise rows.
		const int original_gc_next_pricise_at = gc_next_precise_at_;
		int next_distance = nposm;
		if (row > 0) {
			ttoggle_panel* widget = panel_from_children(row - 1);
			next_distance = widget->gc_distance_ + widget->gc_height_;
		}
		gc_next_precise_at_ = row;
		for (; gc_next_precise_at_ < rows; gc_next_precise_at_ ++) {
			ttoggle_panel* current_gc_row = panel_from_children(gc_next_precise_at_);
			if (current_gc_row->gc_height_ == nposm) {
				break;
			} else {
				current_gc_row->gc_distance_ = next_distance;
			}
			next_distance = current_gc_row->gc_distance_ + current_gc_row->gc_height_;
		}
		if (insert && gc_next_precise_at_ < original_gc_next_pricise_at + count) {
			// when insert, inserted node maybe gc_height_ == nposm, then bottom part require from precise to estimated. 
			const int end = original_gc_next_pricise_at + count;
			for (int at = row + count; at < end; at ++) {
				ttree_node* widget = children[at];
				VALIDATE(gc_is_precise(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
				widget->gc_distance_ = gc_join_estimated(widget->gc_distance_);
			}
		}
	}

	// 1)reset all effected gc_distance_
	if (!insert && gc_first_at_ != nposm) {
		const int orginal_gc_first_at_ = gc_first_at_;
		const int orginal_gc_last_at_ = gc_last_at_;
		int garbaged_height = 0;

		for (int n = 0; n < count; n ++) {
			// if gc_first_at_ is in [0, n], increase it.
			// if gc_last_at_ is in [0, n], will nposm it. if not, 1)if gc_first_at_ in [0, n], decrease it. 2) do nothing.
			ttree_node* widget = nodes[n];
			widget->gc_distance_ = nposm;
			if (gc_first_at_ != nposm) {
				const int should_at = row + n;
				if (should_at >= gc_first_at_ && should_at <= gc_last_at_) {
					garbage_collection(*widget);

					VALIDATE(widget->gc_height_ != nposm, null_str);
					garbaged_height += widget->gc_height_;
				}
			}
		}

		int first_distance = nposm;
		if (gc_first_at_ < row) {
			if (gc_last_at_ >= row) {
				gc_last_at_ = gc_last_at_ < row + count? (row - 1): (gc_last_at_ - count);
			}
			if (gc_first_at_ > gc_next_precise_at_) {
				first_distance = gc_distance_value(children[gc_first_at_]->gc_distance_);
			}

		} else if (gc_first_at_ < row + count) {
			if (gc_last_at_ < row + count) {
				gc_last_at_ = gc_first_at_ = nposm;
			} else {
				gc_last_at_ -= count;
				gc_first_at_ = row;

				if (gc_first_at_ > gc_next_precise_at_) {
					first_distance = gc_distance_value(children[gc_first_at_]->gc_distance_) - garbaged_height;
					VALIDATE(first_distance >= 0, null_str);
				}
			}
		} else {
			gc_first_at_ -= count;
			gc_last_at_ -= count;

			if (gc_first_at_ > gc_next_precise_at_) {
				first_distance = gc_distance_value(children[gc_first_at_]->gc_distance_) - garbaged_height;
				VALIDATE(first_distance >= 0, null_str);
			}
		}
		// update gc_distance_
		if (first_distance != nposm && gc_first_at_ > gc_next_precise_at_) {
			int next_distance = first_distance;
			for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
				ttree_node* widget = children[at];
				widget->gc_distance_ = gc_join_estimated(next_distance);
				next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;	
			}
		}
	}

	// session2: adjust both gc_first_at_ and gc_last_at to right position.
	if (row < gc_first_at_) {
		if (insert) {
			gc_first_at_ += count;
			gc_last_at_ += count;
		} else {
			// gc_first_at_ -= count;
			// gc_last_at_ -= count;
		}

	} else if (row >= gc_first_at_ && row <= gc_last_at_) {
		if (insert) {
			// of couse, can fill toggle_panel of row, and expand [gc_first_at_, gc_last_at_].
			// but if use it, if app insert in [gc_first_at_, gc_last_at_] conitnue, it will slower and slower!
			// so use decrease [gc_first_at_, gc_last_at_].
			if (row == gc_first_at_) {
				gc_first_at_ += count;
				gc_last_at_ += count;
			} else {
				// insert. |->row   row<-|
				const int diff_first = row - gc_first_at_;
				const int diff_last = gc_last_at_ - row;
				// if (diff_first < diff_last && !gc_insert_first_at_lock_) {
				if (false) {
					// when unfold, in order to keep top still, don't modify gc_first_at_.
					gc_last_at_ += count;
					for (int at = gc_first_at_; at < row; at ++) {
						ttoggle_panel* widget = panel_from_children(at);
						garbage_collection(*widget);
					}
					VALIDATE(row < gc_last_at_, null_str);
					gc_first_at_ = row + count;

				} else {
					for (int at = row + count; at <= gc_last_at_ + count; at ++) {
						ttoggle_panel* widget = panel_from_children(at);
						garbage_collection(*widget);
					}

					VALIDATE(row > gc_first_at_, null_str);
					gc_last_at_ = row - 1;
				}
			}
			
		} else {
/*
			// erase.
			if (row == gc_first_at_) {
				gc_first_at_ = gc_last_at_ = nposm;
			} else if (gc_last_at_ - gc_first_at_ < count) {
				gc_last_at_ = gc_first_at;
			}
*/
		}
	}

	if (gc_first_at_ != nposm) {
		// if gc_first_at, gc_last_at_ > gc_next_precise_at_, distance require to estimated.
		if (gc_first_at_ <= gc_next_precise_at_) {
			VALIDATE(gc_last_at_ < gc_next_precise_at_, null_str);
		} else if (gc_is_precise(children[gc_first_at_]->gc_distance_)) {
			for (int at = gc_first_at_; at <= gc_last_at_; at ++) {
				ttree_node* widget = children[at];
				widget->gc_distance_ = gc_join_estimated(widget->gc_distance_);
			}
		}
	}
}

void ttree::layout_init2()
{
	if (gc_calculate_best_size_) {
		return;
	}

	const int rows = gc_nodes_.second;
	VALIDATE(rows && gc_first_at_ != nposm, null_str);
	VALIDATE(!gc_calculate_best_size_ && content_->get_width() > 0, null_str);
	VALIDATE(!linked_max_size_changed_, null_str);

	// tgrid* header = find_widget<tgrid>(content_grid_, "_header_grid", true, false);
	ttree_node** children = gc_lookup_;
	std::set<int> get_best_size_rows;
	{
		tlink_group_owner_lock lock(*this);
/*
		// header
		header->layout_init(true);
		if (row_layout_size_changed_) {
			// set flag. this row require get_best_size again.
			header->set_width(0);
			row_layout_size_changed_ = false;
		}
*/
		// body
		for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
			ttoggle_panel* widget = panel_from_children(row);
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

	// layout_linked_widgets(linked_max_size_changed_);

	// maybe result width changed.
	int max_width = content_->get_width();
	int min_height_changed_at = nposm;
/*
	// header
	if (linked_max_size_changed_ || !header->get_width()) {
		tpoint size = header->get_best_size();
		if (size.x > max_width) {
			max_width = size.x;
		}
		header->twidget::set_size(tpoint(0, size.y));
	}
*/
	for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
		ttoggle_panel* widget = panel_from_children(row);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (linked_max_size_changed_ || get_best_size_rows.count(row)) {
			VALIDATE(false, null_str); // now don't support linked_group, must not enter it.
			ttoggle_panel& panel = *widget;

			{
				tclear_restrict_width_cell_size_lock lock;
				panel.get_best_size();
			}
			tpoint size = panel.calculate_best_size_bh(content_->get_width());
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

	const bool width_changed = max_width != content_grid_->get_width();
/*
	if (width_changed || !header->get_width()) {
		header->place(header->get_origin(), tpoint(max_width, header->get_height()));
	}
*/
	for (int row = gc_first_at_; row <= gc_last_at_; ++ row) {
		ttoggle_panel* widget = panel_from_children(row);
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (width_changed || !widget->get_width()) {
			VALIDATE(widget->gc_height_ != nposm, null_str);
			widget->place(widget->get_origin(), tpoint(max_width, widget->gc_height_));
		}
	}

	if (width_changed) {
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

	if (min_height_changed_at != nposm) {
		int next_distance;
		for (int at = min_height_changed_at; at < gc_next_precise_at_; at ++) {
			ttoggle_panel* widget = panel_from_children(at);
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

void ttree::tgrid2::layout_init(bool linked_group_only)
{
	// normal flow, do nothing.
}

tpoint ttree::tgrid2::calculate_best_size() const
{
	VALIDATE(rows_ == 1 && cols_ == 1, null_str);
/*
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);
*/
	row_height_.clear();
	row_height_.resize(rows_, 0);
	col_width_.clear();
	col_width_.resize(cols_, 0);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	if (!tree_.gc_nodes_.second) {
		return tpoint(0, 0);
	}

	if (tree_.gc_first_at_ == nposm) {
		// tree's best size always is (0, 0).
		return tpoint(0, 0);
/*
		tgc_calculate_best_size_lock lock(tree_);
		tree_.mini_handle_gc(0, 0);
		VALIDATE(tree_.gc_first_at_ != nposm, null_str);
*/
	}

	ttree_node** children = tree_.gc_lookup_;
	int max_width = 0, total_height = 0;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; ++ row) {
		ttoggle_panel* widget = panel_from_children(row);
		VALIDATE(widget, null_str);

		const int width = widget->gc_width_;
		if (width > max_width) {
			max_width = width;
		}

		VALIDATE(widget->gc_height_ != nposm, null_str);
		total_height += widget->gc_height_;
	}
	col_width_[0] = max_width;
	row_height_[0] = tree_.gc_calculate_total_height();

	return tpoint(col_width_[0], row_height_[0]);
}

tpoint ttree::tgrid2::calculate_best_size_bh(const int width)
{
	return tpoint(nposm, nposm);
}

void ttree::tgrid2::place(const tpoint& origin, const tpoint& size)
{
	twidget::place(origin, size);
	tree_.root_node_->twidget::place(origin, size);
	if (!tree_.gc_nodes_.second) {
		return;
	}

	VALIDATE(tree_.gc_first_at_ != nposm, null_str);
	VALIDATE(tree_.gc_current_content_width_ != nposm, null_str);

	VALIDATE(cols_ == 1, null_str);
	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	ttree_node** children = tree_.gc_lookup_;
	tpoint orig = origin;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; ++ row) {
		ttree_node* widget = children[row];
		VALIDATE(widget, null_str);

		orig.y = origin.y + gc_distance_value(widget->gc_distance_);
		widget->place(orig, tpoint(size.x, widget->gc_height_));
	}
}

void ttree::tgrid2::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);
	tree_.root_node_->twidget::set_origin(origin);

	if (!tree_.gc_nodes_.second) {
		return;
	}
	VALIDATE(tree_.gc_first_at_ != nposm, null_str);

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];
		VALIDATE(widget, null_str);
		// distance  
		//    0    ---> origin.y
		//    1    ---> origin.y + 1
		widget->set_origin(tpoint(origin.x, origin.y + gc_distance_value(widget->gc_distance_)));
	}
}

void ttree::tgrid2::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);
	tree_.root_node_->twidget::set_visible_area(area);

	if (!tree_.gc_nodes_.second) {
		return;
	}

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		widget->set_visible_area(area);
	}
}

void ttree::tgrid2::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);
	clear_dirty();

	if (!tree_.gc_nodes_.second) {
		return;
	}
	tree_.root_node_->clear_dirty();
	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}

		if (widget->get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		widget->draw_background(frame_buffer, x_offset, y_offset);
		widget->draw_children(frame_buffer, x_offset, y_offset);
		widget->clear_dirty();
	}
}

void ttree::tgrid2::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	
	if (!tree_.gc_nodes_.second) {
		return;
	}

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		if (widget->get_visible() != VISIBLE) {
			continue;
		}

		widget->dirty_under_rect(clip);
	}
}

twidget* ttree::tgrid2::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}
	
	if (!tree_.gc_nodes_.second) {
		return nullptr;
	}

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		if (widget->get_visible() != VISIBLE) {
			continue;
		}

		twidget* widget2 = widget->find_at(coordinate, must_be_active);
		if (widget2) {
			return widget2;
		}
	}
	return nullptr;
}

void ttree::tgrid2::layout_children()
{
	if (!tree_.gc_nodes_.second) {
		return;
	}

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		widget->layout_children();
	}
}

void ttree::tgrid2::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	if (!tree_.gc_nodes_.second) {
		return;
	}

	twindow* window = get_window();
	std::vector<std::vector<twidget*> >& dirty_list = window->dirty_list();

	VALIDATE(!call_stack.empty() && call_stack.back() == this, null_str);

	ttree_node** children = tree_.gc_lookup_;
	for (int row = tree_.gc_first_at_; row <= tree_.gc_last_at_; row ++) {
		ttree_node* widget = children[row];

		int origin_dirty_list_size = dirty_list.size();

		std::vector<twidget*> child_call_stack = call_stack;
		widget->populate_dirty_list(caller, child_call_stack);
	}
}

tgrid* ttree::mini_create_content_grid()
{
	grid2_ = new tgrid2(*this);
	return grid2_;
}

void ttree::mini_finalize_subclass()
{
	content_grid()->set_rows_cols(1, 1);
	content_grid()->set_child(
			  root_node_
			, 0
			, 0
			, tgrid::VERTICAL_ALIGN_EDGE
				| tgrid::HORIZONTAL_ALIGN_EDGE
			, 0);
}

const std::string& ttree::get_control_type() const
{
	static const std::string type = "tree";
	return type;
}

void ttree::signal_handler_left_button_down(const event::tevent event)
{
	get_window()->keyboard_capture(this);
}

} // namespace gui2

