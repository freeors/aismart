/* $Id: tree.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TREE_HPP_INCLUDED
#define GUI_WIDGETS_TREE_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"
#include "gui/auxiliary/window_builder/tree.hpp"
#include "gui/widgets/tree_node.hpp"

namespace gui2 {

class ttree: public tscroll_container
{
	friend struct implementation::tbuilder_tree;
	friend class ttree_node;

public:
	class tgrid2: public tgrid
	{
		friend class ttree;
	public:
		tgrid2(ttree& tree_view)
			: tree_(tree_view)
		{}

	private:
		void layout_init(bool linked_group_only) override;
		tpoint calculate_best_size() const override;
		tpoint calculate_best_size_bh(const int width) override;
		void place(const tpoint& origin, const tpoint& size) override;
		void set_origin(const tpoint& origin) override;	
		void set_visible_area(const SDL_Rect& area) override;
		void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset) override;
		void dirty_under_rect(const SDL_Rect& clip) override;
		twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;
		void layout_children() override;
		void child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack) override;

		twidget* find(const std::string& id, const bool must_be_active) { return nullptr; }
		const twidget* find(const std::string& id, const bool must_be_active) const { return nullptr; }

	private:
		ttree& tree_;
	};

	typedef implementation::tbuilder_tree::tnode tnode_definition;

	/**
	 * Constructor.
	 *
	 * @param has_minimum         Does the listbox need to have one item
	 *                            selected.
	 * @param has_maximum         Can the listbox only have one item
	 *                            selected.
	 * @param placement           How are the items placed.
	 * @param select              Select an item when selected, if false it
	 *                            changes the visible state instead.
	 */
	explicit ttree(const std::vector<tnode_definition>& node_definitions, const bool foldable);
	~ttree();

	void enable_select(const bool enable);

	ttree_node& get_root_node() { return *root_node_; }

	ttree_node& insert_node(const std::string& id, const std::map<std::string, std::string>& data, int at = nposm, const bool branch = false);
	void erase_node(ttree_node& node);

	void scroll_to_node(const ttree_node& node);
	void clear() { root_node_->erase_children(); }

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/) {}

	bool empty() const;

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_indention_step_size(const unsigned indention_step_size)
	{
		indention_step_size_ = indention_step_size;
	}

	ttree_node* cursel() const { return cursel_; }
	void select_node(ttree_node* node);

	void set_did_fold_changed(const boost::function<void(ttree&, ttree_node&, const bool)>& did)
	{
		did_fold_changed_ = did;
	}
	void set_did_node_changed(const boost::function<void(ttree& view, ttree_node& node)>& did)
	{
		did_node_changed_ = did;
	}
	void set_did_double_click(const boost::function<void(ttree& view, ttree_node& node)>& did)
	{
		did_double_click_ = did;
	}
	void set_did_right_button_up(const boost::function<void(ttree& view, ttree_node&, const tpoint&)>& did)
	{
		did_right_button_up_ = did;
	}

	void set_left_align() { left_align_ = true; } 

	void set_no_indentation(bool val) { no_indentation_ = val; }


	/** Inherited from tscroll_container. */
	tpoint adjust_content_size(const tpoint& size);
	void adjust_offset(int& x_offset, int& y_offset);

private:
	void layout_init2() override;
	void show_row_rect(const int at) override;
	// tree's best_size always is (0, 0).
	// tpoint calculate_best_size() const override { return tpoint(0, 0); }

	tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) override;

	tgrid* mini_create_content_grid() override;
	void mini_finalize_subclass() override;

	void garbage_collection(ttoggle_panel& row);

	void gc_insert_cache_resize(int size);

	void gc_lookup_resize(int size, const int vsize);
	int gc_lookup_find(const ttree_node* node) const;
	void gc_lookup_insert(ttree_node& node, const ttree_node* back_node, const bool only_children);
	void gc_lookup_erase(ttree_node& node, const bool only_children, const bool must_include_children);
	void gc_lookup_reset();

	void gc_insert_or_erase_row(const int row, const bool insert, ttree_node** nodes, const int count);

	void erase_node_internal(ttree_node& node, const bool erase_children);

	int gc_handle_rows() const override { return gc_nodes_.second; }
	tpoint gc_handle_calculate_size(ttoggle_panel& widget, const int width) const override;
	ttoggle_panel* gc_widget_from_children(const int at) const override;
	int gc_handle_update_height(const int new_height) override;
	void validate_children_continuous() const override;

	void invalidate_layout(twidget* widget) override;

private:

	tgrid2* grid2_;
	/**
	 * @todo evaluate which way the dependancy should go.
	 *
	 * We no depend on the implementation, maybe the implementation should
	 * depend on us instead.
	 */
	const std::vector<tnode_definition> node_definitions_;

	unsigned indention_step_size_;

	ttree_node* root_node_;

	bool foldable_;
	bool selectable_;
	ttree_node* cursel_;

	// first: precise_nodes, second: displayed nodes.
	std::pair<int, int> gc_nodes_;

	ttree_node** gc_lookup_;
	int gc_lookup_size_;

	class tcalculate_gc_insert_cache_vsize_lock
	{
	public:
		tcalculate_gc_insert_cache_vsize_lock(ttree& tree_view, const bool insert)
			: tree_view_(tree_view)
		{
			VALIDATE(!tree_view_.gc_insert_cache_vsize_, null_str);
			VALIDATE(!tree_view_.gc_insert_or_erase_, null_str);
			tree_view_.gc_insert_or_erase_ = insert;
		}
		~tcalculate_gc_insert_cache_vsize_lock()
		{
			tree_view_.gc_insert_cache_vsize_ = 0;
			tree_view_.gc_insert_or_erase_ = false;
		}

	private:
		ttree& tree_view_;
	};

	ttree_node** gc_insert_cache_;
	int gc_insert_cache_size_;
	int gc_insert_cache_vsize_;
	bool gc_insert_or_erase_;

	class tgc_insert_first_at_lock
	{
	public:
		tgc_insert_first_at_lock(ttree& tree_view)
			: tree_view_(tree_view)
		{
			VALIDATE(!tree_view_.gc_insert_first_at_lock_, null_str);
			tree_view_.gc_insert_first_at_lock_ = true;
		}
		~tgc_insert_first_at_lock()
		{
			tree_view_.gc_insert_first_at_lock_ = false;
		}

	private:
		ttree& tree_view_;
	};
	bool gc_insert_first_at_lock_;

	bool linked_max_size_changed_;
	bool row_layout_size_changed_;

	boost::function<void (ttree&, ttree_node&, const bool)> did_fold_changed_;
	boost::function<void (ttree&, ttree_node&)> did_node_changed_;
	// only leaf can trigger did_double_click_.
	boost::function<void (ttree&, ttree_node&)> did_double_click_;
	boost::function<void (ttree&, ttree_node&, const tpoint&)> did_right_button_up_;

	bool left_align_;
	bool no_indentation_;

	/***** ***** ***** signal handlers ***** ****** *****/
	void signal_handler_left_button_down(const event::tevent event);

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

