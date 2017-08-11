/* $Id: tree_view_node.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/tree_node.hpp"

#include "gettext.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

ttree_node::ttree_node(const std::string& id
		, const std::vector<tnode_definition>& node_definitions
		, ttree_node* parent_node
		, ttree& parent_tree_view
		, const std::map<std::string, std::string>& data
		, bool branch)
	: ttoggle_panel()
	, parent_node_(parent_node)
	, tree_(parent_tree_view)
	, children_()
	, node_definitions_(node_definitions)
	, icon_(nullptr)
	, branch_(branch)
{
	terminal_ = false;
	if (id != "root") {
		BOOST_FOREACH(const tnode_definition& node_definition, node_definitions_) {
			if (node_definition.id == id) {
				set_parent(tree_.root_node_); // all node's parent is root_node_.
				
				node_definition.builder->build2(*this);
				set_child_members(data);

				tgrid& grid2 = grid();

				icon_ = find_widget<ttoggle_button>(
						  &grid2
						, "__icon"
						, false
						, false);

				if (icon_) {
					VALIDATE(tree_.foldable_, null_str);
					if (icon_->label().empty()) {
						// default to icon.
						icon_->set_label("fold-common");
					}
					tvisible visible = twidget::HIDDEN;
					if (branch_) {
						visible = twidget::VISIBLE;
					} else if (parent_tree_view.no_indentation_ && get_indention_level() >= 2) {
						visible = twidget::INVISIBLE;
					}
					icon_->set_visible(visible);
					icon_->set_value(true);
					icon_->set_did_state_changed(boost::bind(&ttree_node::did_icon_state_changed, this, _1));

				} else {
					VALIDATE(!tree_.foldable_, null_str);
				}

				if (parent_node_ && parent_node_->icon_) {
					parent_node_->icon_->set_visible(twidget::VISIBLE);
				}
				return;
			}
		}

		VALIDATE(false, _("Unknown builder id for tree view node."));

	} else {
		set_parent(parent_tree_view.content_grid()); // let content_grid_ receive event.
		set_id(id);
	}
}

ttree_node::~ttree_node()
{
	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		delete *it;
	}

	if (tree_.cursel_ == this) {
		tree_.cursel_ = nullptr;
	}
}

ttree_node& ttree_node::insert_node(const std::string& id, const std::map<std::string, std::string>& data, int at, const bool branch)
{
	tdisable_invalidate_layout_lock lock;

	if (at == nposm) {
		at = children_.size();
	}
	VALIDATE(at <= (int)children_.size(), null_str);

	std::vector<ttree_node*>::iterator it = children_.end();
	if (at < (int)children_.size()) {
		it = children_.begin() + at;
	}

	it = children_.insert(it, new ttree_node(
			  id
			, node_definitions_
			, this
			, tree_
			, data
			, branch));
	ttree_node& ret = **it;
	ret.at_ = at;

	for (++ it; it != children_.end(); ++ it) {
		ttree_node& node = **it;
		node.at_ ++;
	}

	if (is_folded()) {
		return ret;
	}
	if (parent_node_ && !gc_is_shown()) {
		return ret;
	}

	const ttree_node* back_node = nullptr;
	if (ret.at_) {
		back_node = gc_terminal_node(ret.at_ - 1);
	} else if (parent_node_) {
		back_node = this;
	}
	tree_.gc_lookup_insert(ret, back_node, false);
	tree_.invalidate_layout(nullptr);

	return ret;
}

ttree_node& ttree_node::child(int at) const
{
	if (at == nposm) {
		at = children_.size() - 1;
	}
	VALIDATE(at >= 0 && at < (int)children_.size(), null_str);
	return *children_[at];
}

unsigned ttree_node::get_indention_level() const
{
	unsigned level = 0;

	const ttree_node* node = this;
	while(!node->is_root_node()) {
		node = &node->parent_node();
		++level;
	}

	return level;
}

bool ttree_node::is_front(const ttree_node& that) const
{
	VALIDATE(this != &that, null_str);
	if (parent_node_ == that.parent_node_) {
		return at_ < that.at_;
	}
	const int this_level = get_indention_level();
	const int that_level = that.get_indention_level();
	int diff = this_level - that_level;

	// same level, but diffrent parent.
	const ttree_node* this_node = nullptr;
	const ttree_node* that_node = nullptr;

	if (diff < 0) {
		const ttree_node* node = &that;
		while (diff) {
			node = &node->parent_node();
			diff ++;
		}
		if (this == node) {
			// that is this's child.
			return true;
		}

		this_node = this;
		that_node = node;
		// return panel_->at_ < node->panel_->at_;

	} else if (diff > 0) {
		const ttree_node* node = this;
		while (diff) {
			node = &node->parent_node();
			diff --;
		}
		if (node == &that) {
			// this is that's child.
			return false;
		}
		this_node = node;
		that_node = &that;
		// return node->panel_->at_ < that.panel_->at_;

	} else {
		// same level, but diffrent parent.
		this_node = this;
		that_node = &that;

	}

	const ttree_node* this_parent = &this_node->parent_node();
	const ttree_node* that_parent = &that_node->parent_node();

	while (this_parent != that_parent) {
		this_node = this_parent;
		that_node = that_parent;
		this_parent = &this_node->parent_node();
		that_parent = &that_node->parent_node();

	}
	return this_node->at_ < that_node->at_;
}

ttree_node* ttree_node::gc_terminal_node(const int at) const
{
	ttree_node* ret = children_[at];
	if (!ret->is_folded() && !ret->children_.empty()) {
		return ret->gc_terminal_node(ret->children_.size() - 1);
	}
	return ret;
}

void ttree_node::calculate_gc_insert_cache_vsize(const bool only_children, const bool must_include_children)
{
	if (!only_children) {
		tree_.gc_insert_cache_resize(tree_.gc_insert_cache_vsize_ + sizeof(ttree_node*));
		tree_.gc_insert_cache_[tree_.gc_insert_cache_vsize_ / sizeof(ttree_node*)] = this;
		tree_.gc_insert_cache_vsize_ += sizeof(ttree_node*);
	}

	if (!must_include_children && is_folded()) {
		return;
	}

	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node* node = *it;

		tree_.gc_insert_cache_resize(tree_.gc_insert_cache_vsize_ + sizeof(ttree_node*));
		tree_.gc_insert_cache_[tree_.gc_insert_cache_vsize_ / sizeof(ttree_node*)] = node;
		tree_.gc_insert_cache_vsize_ += sizeof(ttree_node*);
		if (!node->is_folded()) {
			// above has calculated nodeslef, so only_children is true.
			// will enter second or more, must consider is_folded falg, so must_include_children is false.
			node->calculate_gc_insert_cache_vsize(true, false);
		}
	}
}

bool ttree_node::gc_is_shown() const
{
	VALIDATE(parent_node_, null_str);

	const ttree_node* node = &parent_node();

	while (node) {
		if (node->is_folded()) {
			return false;
		}
		node = node->is_root_node()? nullptr: &node->parent_node();
	}
	return true;
}

const ttree_node& ttree_node::gc_serie_first_shown() const
{
	VALIDATE(parent_node_, null_str);

	const ttree_node* ret = this;
	const ttree_node* tmp = parent_node_;

	while (tmp) {
		if (tmp->is_folded()) {
			ret = tmp;
		}
		tmp = tmp->parent_node_;
	}
	return *ret;
}

ttree_node& ttree_node::gc_serie_first_shown()
{
	VALIDATE(parent_node_, null_str);

	ttree_node* ret = this;
	ttree_node* tmp = parent_node_;

	while (tmp) {
		if (tmp->is_folded()) {
			ret = tmp;
		}
		tmp = tmp->parent_node_;
	}
	return *ret;
}

void ttree_node::set_widget_visible(const std::string& id, const bool visible)
{
	twidget* widget = find_widget<twidget>(&grid(), id, false, true);
	twidget::tvisible original = widget->get_visible();
	VALIDATE(original != twidget::HIDDEN, null_str);
	if ((visible && original == twidget::VISIBLE) || (!visible && original == twidget::INVISIBLE)) {
		return;
	}

	{
		tdisable_invalidate_layout_lock lock;
		widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
	}
}

void ttree_node::set_widget_label(const std::string& id, const std::string& label)
{
	tcontrol* widget = find_widget<tcontrol>(&grid(), id, false, true);
	if (widget->label() == label) {
		return;
	}
	widget->set_label(label);
}

void ttree_node::post_fold_changed(const bool folded)
{
	VALIDATE(parent_node_, null_str);

	// ttree::tgc_insert_first_at_lock lock(tree_view_);

	if (folded) {
		if (tree_.cursel_ && is_child2(*tree_.cursel_)) {
			tree_.select_node(this);
		}

		if (!children_.empty() && gc_is_shown()) {
			tree_.gc_lookup_erase(*this, true, true);
		}

	} else {
		if (!children_.empty() && gc_is_shown()) {
			tree_.gc_lookup_insert(*this, nullptr, true);
		}
	}

	if (tree_.did_fold_changed_) {
		tree_.did_fold_changed_(tree_, *this, folded);
	}

	tree_.validate_children_continuous();

	if (tree_.gc_current_content_width_ > 0 && tree_.gc_next_precise_at_ > 0 && !folded) {
		tree_.scroll_to_node(*this);
		// const ttree_node& node2 = gc_serie_first_shown();
		// tree_view_.gc_locked_at_ = tree_view_.gc_lookup_find(&node2);

	} else {
		tree_.invalidate_layout(nullptr);
	}

	// tree_view_.invalidate_layout(nullptr);
}

bool ttree_node::is_folded() const
{
	return icon_ && icon_->get_value();
}

void ttree_node::fold()
{
	if (!tree_.foldable_) {
		return;
	}

	VALIDATE(parent_node_, null_str);
	if (is_folded()) {
		return;
	}
	VALIDATE(!icon_ || !icon_->get_value(), null_str);
	if (empty() && !branch_) {
		return;
	}
	if (icon_) {
		icon_->set_value(true);
	}
}

void ttree_node::unfold()
{
	VALIDATE(parent_node_, null_str);
	if (!is_folded()) {
		return;
	}
	VALIDATE(!icon_ || icon_->get_value(), null_str);
	if (empty() && !branch_) {
		return;
	}
	if (icon_) {
		icon_->set_value(false);
	}
}

void ttree_node::fold_children()
{
	if (!tree_.foldable_) {
		return;
	}

	for (std::vector<ttree_node*>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;
		node.fold();
	}
}

void ttree_node::unfold_children()
{
	for (std::vector<ttree_node*>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;

		if (node.get_visible() == twidget::INVISIBLE) {
			continue;
		}

		node.unfold();
	}
}

bool ttree_node::is_child2(const ttree_node& target) const
{
	const ttree_node* parent = &target;
	while (!parent->is_root_node()) {
		parent = &parent->parent_node();
		if (this == parent) {
			return true;
		}
	}
	return false;
}

void ttree_node::sort_children(const boost::function<bool (const ttree_node&, const ttree_node&)>& did_compare) 
{
	if (children_.size() < 2) {
		return;
	}
	std::stable_sort(children_.begin(), children_.end(), tsort_func(did_compare));
	int at = 0;
	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;
		node.at_ = at ++;
	}

	if (!is_folded() && (!parent_node_ || gc_is_shown())) {
		//
		// resort node belong this in gc_lookup_.
		// 
		ttree::tcalculate_gc_insert_cache_vsize_lock lock(tree_, true);
		calculate_gc_insert_cache_vsize(true, false);

		const int count = tree_.gc_insert_cache_vsize_ / sizeof(ttree_node*);

		int hit_at = tree_.gc_lookup_find(this);
		memcpy(&tree_.gc_lookup_[hit_at + 1], tree_.gc_insert_cache_, tree_.gc_insert_cache_vsize_);
	}

	tree_.invalidate_layout(nullptr);
}

void ttree_node::erase_children()
{
	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		// delete *it;
		tree_.erase_node_internal(**it, true);
	}
	children_.clear();

	tree_.validate_children_continuous();
	tree_.invalidate_layout(nullptr);
}

void ttree_node::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	if (parent_node_) {
		ttoggle_panel::dirty_under_rect(clip);
	}
	if (is_folded()) {
		return;
	}

	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;
		if (node.get_visible() != twidget::VISIBLE) {
			continue;
		}
		node.dirty_under_rect(clip);
	}
}

twidget* ttree_node::find_at(const tpoint& coordinate, const bool must_be_active)
{
	VALIDATE(parent_node_, null_str);
	return ttoggle_panel::find_at(coordinate, must_be_active);
}

twidget* ttree_node::find(const std::string& id, const bool must_be_active)
{
	VALIDATE(parent_node_, null_str);
	return ttoggle_panel::find(id, must_be_active);
}

const twidget* ttree_node::find(const std::string& id, const bool must_be_active) const
{
	VALIDATE(parent_node_, null_str);
	return ttoggle_panel::find(id, must_be_active);
}

void ttree_node::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	VALIDATE(parent_node_, null_str);
	if (parent_node_) {
		ttoggle_panel::child_populate_dirty_list(caller, call_stack);
	}
}

tpoint ttree_node::calculate_best_size() const
{
	VALIDATE(parent_node_, null_str);
	tpoint ret = calculate_best_size(get_indention_level() - 1, tree_.indention_step_size_);

	int left_margin = 0;
/*
	has bug, fixed in future.
	const tspace4 conf_margin = mini_conf_margin();
	left_margin = explicit_margin_.left != nposm? explicit_margin_.left: conf_margin.left;
*/
	ret.x += left_margin;
	return ret;
}

tpoint ttree_node::calculate_best_size(const int indention_level, const unsigned indention_step_size) const
{
	if (tree_.left_align_) {
		return calculate_best_size_left_align(indention_level, indention_step_size);
	}

	tpoint best_size = ttoggle_panel::calculate_best_size();
	if (indention_level > 0) {
		best_size.x += indention_level * indention_step_size;
	}
	return best_size;
}

tpoint ttree_node::calculate_best_size_left_align(const int indention_level
		, const unsigned indention_step_size) const
{
	tpoint best_size = ttoggle_panel::get_best_size();
	if(indention_level > 0) {
		best_size.x += indention_step_size;
	}

	if(is_folded()) {

		// Folded grid return own best size 'best_size'.
		return best_size;
	}

	int max_node_width = 0;
	int node_height = 0;
	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		const ttree_node& node = **it;

		if(node.get_visible() == twidget::INVISIBLE) {
			continue;
		}

		const tpoint node_size = node.calculate_best_size_left_align(indention_level + 1,
				indention_step_size);

		// if (is_root_node() || itor != children_.begin()) {
		//	best_size.y += node_size.y;
		// }
		max_node_width = std::max(max_node_width, node_size.x);
		node_height += node_size.y;
	}
	best_size.x += max_node_width;
	best_size.y = std::max(node_height, best_size.y);;
	
	return best_size;
}

void ttree_node::set_origin(const tpoint& origin)
{
	VALIDATE(parent_node_, "Root node doesn't use normal set_origin!");
	ttoggle_panel::set_origin(origin);
}

void ttree_node::place(const tpoint& origin, const tpoint& size)
{
	VALIDATE(parent_node_, "Root node doesn't use normal place!");

	const int left_space = (get_indention_level() - 1) * tree_.indention_step_size_;

	int left_margin = 0;
/*
	has bug, fixed in future.
	const tspace4 conf_margin = mini_conf_margin();
	int left_margin = explicit_margin_.left != nposm? explicit_margin_.left: conf_margin.left;
*/

	ttoggle_panel::set_margin(left_margin + left_space, nposm, nposm, nposm);
	ttoggle_panel::place(origin, size);
}

unsigned ttree_node::place_left_align(const unsigned indention_step_size, tpoint origin, unsigned width)
{
	const unsigned offset = origin.y;
	tpoint best_size = ttoggle_panel::get_best_size();
	// grid_size_ = best_size;
	// best_size.x = width;
	ttoggle_panel::place(origin, best_size);

	if(!is_root_node()) {
		origin.x += indention_step_size + ttoggle_panel::get_width();
		width -= indention_step_size + ttoggle_panel::get_width();
	}
	origin.y += best_size.y;

	if(is_folded()) {
		// folded node done.
		return origin.y - offset;
	}

	int index = 0;
	for (std::vector<ttree_node*>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;
		if (index == 0) {
			origin.y -= best_size.y;
		}
		index ++;
		origin.y += node.place_left_align(!node.empty()? indention_step_size: 0, origin, width);
	}

	// Inherited.
	twidget::set_size(tpoint(width, origin.y - offset));

	return origin.y - offset;
}

void ttree_node::set_visible_area(const SDL_Rect& area)
{
	VALIDATE(parent_node_, "Root node doesn't use normal set_visible_area!");

	// twidget::set_visible_area(area);
	ttoggle_panel::set_visible_area(area);
}

void ttree_node::impl_draw_children(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	// VALIDATE(parent_node_, "Root node doesn't use normal impl_draw_children!");

	if (parent_node_) {
		ttoggle_panel::impl_draw_children(frame_buffer, x_offset, y_offset);
	} else {
		tree_.content_grid()->impl_draw_children(frame_buffer, x_offset, y_offset);
	}
/*
	if (is_folded()) {
		return;
	}

	for (std::vector<ttree_node*>::iterator it = children_.begin(); it != children_.end(); ++ it) {
		ttree_node& node = **it;

		if (node.get_visible() != twidget::VISIBLE) {
			continue;
		}

		if (node.get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		node.impl_draw_children(frame_buffer, x_offset, y_offset);
		node.clear_dirty();
	}
*/
}

void ttree_node::clear_texture()
{
	if (parent_node_) {
		ttoggle_panel::clear_texture();
	}
	for (std::vector<ttree_node*>::iterator it = children_.begin(); it != children_.end(); ++it) {
		ttree_node& node = **it;
		node.clear_texture();
	}
}

void ttree_node::validate_children_continuous(int& next_shown_at, int& next_distance, const bool parent_has_folded) const
{
	if (parent_node_) {
		if (parent_has_folded) {
			const tgrid& grid = layout_grid();
			VALIDATE(grid.canvas_texture_is_null(), null_str);
		}
	}

	int next_at = 0;
	for (std::vector<ttree_node*>::const_iterator it = children_.begin(); it != children_.end(); ++ it) {
		const ttree_node* widget = *it;

		// all row must visible
		VALIDATE(widget->get_visible() == twidget::VISIBLE, null_str);
		VALIDATE(widget->at_ == next_at ++, null_str);

		if (is_folded() || parent_has_folded) {
			// foled node must not exist canvas.
			const tgrid& grid = widget->layout_grid();
			VALIDATE(grid.canvas_texture_is_null(), null_str);
		}

		if (!is_folded() && !parent_has_folded) {
			VALIDATE(tree_.gc_lookup_[next_shown_at] == widget, null_str);

			// distance
			if (next_shown_at < tree_.gc_next_precise_at_) {
				VALIDATE(gc_is_precise(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
				VALIDATE(widget->gc_distance_ == next_distance, null_str);
				next_distance += widget->gc_height_;

			} else if (next_shown_at == tree_.gc_next_precise_at_) {
				VALIDATE(widget->gc_distance_ == nposm && widget->gc_height_ == nposm, null_str);

			} else if (next_shown_at >= tree_.gc_first_at_ && next_shown_at <= tree_.gc_last_at_) {
				if (next_shown_at == tree_.gc_first_at_) {
					VALIDATE(gc_is_estimated(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
					next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;

				} else {
					VALIDATE(gc_is_estimated(widget->gc_distance_) && widget->gc_height_ != nposm, null_str);
					VALIDATE(gc_distance_value(widget->gc_distance_) == next_distance, null_str);
					next_distance = gc_distance_value(widget->gc_distance_) + widget->gc_height_;
				}

			} else {
				// if gc_distance != npos, it's gc_height_ must valid. but if gc_height_ is valid, gc_distance_ maybe npos.
				VALIDATE(widget->gc_distance_ == nposm || widget->gc_height_ != nposm, null_str);
			}

			// at_ must be continues.
			next_shown_at ++;

			// support must one selectable.
			if (widget->get_value()) {
				VALIDATE(widget == tree_.cursel(), null_str);
			} else {
				VALIDATE(widget != tree_.cursel(), null_str);
			}

		} else {
			VALIDATE(!widget->get_value(), null_str);
		}

		widget->validate_children_continuous(next_shown_at, next_distance, parent_has_folded || is_folded());
	}

	if (!parent_node_) {
		VALIDATE(next_shown_at == tree_.gc_nodes_.second, null_str);
	}
}

void ttree_node::did_icon_state_changed(ttoggle_button& widget)
{
	VALIDATE(tree_.foldable_, null_str);

	post_fold_changed(is_folded());
}

void ttree_node::signal_handler_mouse_enter(const event::tevent event, bool& handled)
{
	if (!parent_node_) {
		handled = true;
		return;
	}
	ttoggle_panel::signal_handler_mouse_enter(event, handled);
}

void ttree_node::signal_handler_mouse_leave(const event::tevent event, bool& handled)
{
	if (!parent_node_) {
		handled = true;
		return;
	}
	ttoggle_panel::signal_handler_mouse_leave(event, handled);
}

void ttree_node::signal_handler_left_button_click(const event::tevent event, bool& handled, bool& halt)
{
	handled = halt = true;
	if (!parent_node_) {
		return;
	}

	// We only snoop on the event so normally don't touch the handled, else if
	// we snoop in preexcept when halting.
	if (get_value()) {
		// Forbid deselecting
	} else {
		tree_.select_node(this);
	}
}

void ttree_node::signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt)
{
	handled = halt = true;
	if (!parent_node_) {
		return;
	}

	// We only snoop on the event so normally don't touch the handled, else if
	// we snoop in preexcept when halting.
	bool branch = branch_ || !children_.empty();
	if (branch) {
		if (!get_value()) {
			tree_.select_node(this);
		}

		if (is_folded()) {
			if (icon_) {
				icon_->set_value(false);
			}
		} else {
			if (icon_ && tree_.foldable_) {
				icon_->set_value(true);
			}
		}

	} else {
		if (tree_.did_double_click_) {
			tree_.did_double_click_(tree_, *this);
		}
	}
}

void ttree_node::signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordinate)
{
	handled = halt = true;
	if (!parent_node_) {
		return;
	}

	if (tree_.did_right_button_up_) {
		tree_.did_right_button_up_(tree_, *this, coordinate);
	}
}

} // namespace gui2

