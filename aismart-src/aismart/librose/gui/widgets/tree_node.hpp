/* $Id: tree_view_node.hpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#ifndef GUI_WIDGETS_TREE_NODE_HPP_INCLUDED
#define GUI_WIDGETS_TREE_NODE_HPP_INCLUDED

#include "gui/auxiliary/window_builder/tree.hpp"
#include "gui/widgets/toggle_panel.hpp"

#include <boost/ptr_container/ptr_vector.hpp>

namespace gui2 {

class ttoggle_button;
class ttree;

class ttree_node: public ttoggle_panel
{
	friend class ttree;

public:
	typedef implementation::tbuilder_tree::tnode tnode_definition;
	explicit ttree_node(const std::string& id
			, const std::vector<tnode_definition>& node_definitions
			, ttree_node* parent_node
			, ttree& parent_tree_view
			, const std::map<std::string, std::string>& data
			, bool branch);

	~ttree_node();

	/**
	 * Adds a child item to the list of child nodes.
	 *
	 * @param id                  The id of the node definition to use for the
	 *                            new node.
	 * @param data                The data to send to the set_members of the
	 *                            widgets. If the member id is not an empty
	 *                            string it is only send to the widget that has
	 *                            the wanted id (if any). If the member id is an
	 *                            empty string, it is send to all members.
	 *                            Having both empty and non-empty id's gives
	 *                            undefined behaviour.
	 * @param index               The item before which to add the new item,
	 *                            0 == begin, -1 == end.
	 */
	ttree_node& insert_node(const std::string& id, const std::map<std::string, std::string>& data, int at = nposm, const bool branch = false);

	/**
	 * Removes all child items from the widget.
	 */
	void erase_children();

	/**
	 * Is this node the root node?
	 *
	 * When the parent tree view is created it adds one special node, the root
	 * node. This node has no parent node and some other special features so
	 * several code paths need to check whether they are the parent node.
	 */
	bool is_root_node() const { return parent_node_ == NULL; }

	/**
	 * The indention level of the node.
	 *
	 * The root node starts at level 0.
	 */
	unsigned get_indention_level() const;

	/** Does the node have children? */
	bool empty() const { return children_.empty(); }

	size_t size() const { return children_.size(); }

	ttree_node& child(int at) const;

	/** Is the node folded? */
	bool is_folded() const;

	void set_widget_visible(const std::string& id, const bool visible);
	void set_widget_label(const std::string& id, const std::string& label);

	class tsort_func
	{
	public:
		tsort_func(const boost::function<bool (const ttree_node&, const ttree_node&)>& did_compare)
			: did_compare_(did_compare)
		{}
		bool operator()(ttree_node* a, ttree_node* b) const
		{
			return did_compare_(*a, *b);
		}
	private:
		const boost::function<bool (const ttree_node&, const ttree_node&)>& did_compare_;
	};

	void sort_children(const boost::function<bool (const ttree_node&, const ttree_node&)>& did_compare);

	void fold();
	void unfold();
	void fold_children();
	void unfold_children();

	bool is_child2(const ttree_node& target) const;

	/**
	 * Returns the parent node.
	 *
	 * @pre                       is_root_node() == false.
	 */
	ttree_node& parent_node() 
	{
		VALIDATE(parent_node_, null_str);
		return *parent_node_; 
	}
	const ttree_node& parent_node() const 
	{
		VALIDATE(parent_node_, null_str);
		return *parent_node_; 
	}

	ttree& tree() { return tree_; }
	const ttree& tree() const { return tree_; }

	ttoggle_button* icon() const { return icon_; }

	/** Inherited from twidget.*/
	void dirty_under_rect(const SDL_Rect& clip) override;

	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;
	twidget* find(const std::string& id, const bool must_be_active) override;
	const twidget* find(const std::string& id, const bool must_be_active) const override;

private:
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack) override;

	tpoint calculate_best_size() const override;

	bool disable_click_dismiss() const { return true; }

	tpoint calculate_best_size(const int indention_level, const unsigned indention_step_size) const;

	tpoint calculate_best_size_left_align(const int indention_level
			, const unsigned indention_step_size) const;

	void set_origin(const tpoint& origin) override;

	void place(const tpoint& origin, const tpoint& size) override;

	unsigned place_left_align(
			  const unsigned indention_step_size
			, tpoint origin
			, unsigned width);

	void set_visible_area(const SDL_Rect& area);

	void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset);

	void clear_texture() override;

	void validate_children_continuous(int& next_shown_at, int& next_distance, const bool parent_has_folded) const;

	void did_icon_state_changed(ttoggle_button& widget);
	void post_fold_changed(const bool folded);

	const ttree_node& gc_serie_first_shown() const;
	ttree_node& gc_serie_first_shown();

	// attention: before call is_front, this must not equal that.
	bool is_front(const ttree_node& that) const;

	ttree_node* gc_terminal_node(const int at) const;
	void calculate_gc_insert_cache_vsize(const bool only_children, const bool must_include_children);
	bool gc_is_shown() const;

private:

	/**
	 * Our parent node.
	 *
	 * All nodes except the root node have a parent node.
	 */
	ttree_node* parent_node_;

	/** The tree view that owns us. */
	ttree& tree_;

	/**
	 * Our children.
	 *
	 * We want the returned child nodes to remain stable so store pointers.
	 */
	std::vector<ttree_node*> children_;

	/**
	 * The node definitions known to use.
	 *
	 * This list is needed to create new nodes.
	 *
	 * @todo Maybe store this list in the tree_view to avoid copying the
	 * reference.
	 */
	const std::vector<tnode_definition>& node_definitions_;

	/** The icon to show the folded state. */
	ttoggle_button* icon_;
	bool branch_;

	void signal_handler_mouse_enter(const event::tevent event, bool& handled) override;
	void signal_handler_mouse_leave(const event::tevent event, bool& handled) override;

	void signal_handler_left_button_click(const event::tevent event, bool& handled, bool& halt) override;
	void signal_handler_left_button_double_click(const event::tevent event, bool& handled, bool& halt) override;
	void signal_handler_right_button_up(bool& handled, bool& halt, const tpoint& coordinate) override;
};

} // namespace gui2

#endif


