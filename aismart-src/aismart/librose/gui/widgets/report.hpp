/* $Id: scroll_label.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_REPORT_HPP_INCLUDED
#define GUI_WIDGETS_REPORT_HPP_INCLUDED

#include "gui/widgets/scroll_container.hpp"
#include <boost/function.hpp>

namespace gui2 {

class tspacer;
class tbutton;
class ttoggle_button;
class tpanel;

namespace implementation {
	struct tbuilder_report;
}

/**
 * Label showing a text.
 *
 * This version shows a scrollbar if the text gets too long and has some
 * scrolling features. In general this widget is slower as the normal label so
 * the normal label should be preferred.
 */
class treport : public tscroll_container
{
	friend struct implementation::tbuilder_report;
public:
	class tgrid2: public tgrid
	{
		friend treport;
	public:
		tgrid2(treport& report)
			: tgrid(1, 0)
			, report_(report)
			, last_draw_end_(0, 0)
		{
			set_parent(&report);
			set_id("_content_grid");
		}

	private:
		tpoint calculate_best_size() const override;
		void place(const tpoint& origin, const tpoint& size) override;
		void layout_init(bool linked_group_only) override;

		void report_create_stuff(int unit_w, int unit_y, int gap, bool extendable, int fixed_cols);
		int insert_child(twidget& widget, int at);
		void erase_child(int at);
		void replacement_children();
		void erase_children();
		void hide_children();
		void clear_stuff_widget() { stuff_widget_.clear(); }

		tpoint get_multi_line_reference_size() const;
		void calculate_grid_params(int unit_w, int unit_h, int gap, bool extendable, int fixed_cols);
		void update_last_draw_end();

	private:
		treport& report_;
		tpoint last_draw_end_;
	};

	explicit treport(bool multi_line, bool toggle, const std::string& unit_definition, const std::string& unit_w, const std::string& unit_h, int gap, bool segment_switch, int fixed_cols);

	const tpoint& get_unit_size() const { return unit_size_; }
	int get_unit_width() const { return unit_size_.x; }
	int get_unit_height() const { return unit_size_.y; }

	// below 5 function special to report control.
	tcontrol& insert_item(const std::string& id, const std::string& label, int at = nposm);
	void erase_item(int at = nposm);
	void clear();
	void hide_children();

	void set_item_visible(int at, bool visible);
	bool get_item_visible(int at) const;
	void set_item_visible(const tcontrol& widget, bool visible);
	bool get_item_visible(const tcontrol& widget) const;

	tcontrol& item(int at) const;
	int items() const;

	void select_item(int at);
	void select_item(twidget& widget);
	ttoggle_button* cursel() const;
	void tabbar_set_callback_show(const boost::function<void (treport&, tcontrol&)>& callback) 
	{ 
		callback_show_ = callback; 
	}
	void set_did_item_pre_change(const boost::function<bool (treport&, ttoggle_button&, ttoggle_button* previous)>& did)
	{
		did_item_pre_change_ = did;
	}
	void set_did_item_changed(const boost::function<void (treport&, ttoggle_button&)>& did)
	{
		did_item_changed_ = did;
	}
	void set_did_item_click(const boost::function<void (treport&, tbutton&)>& did)
	{
		did_item_click_ = did;
	}

	int get_at2(const void* cookie) const;
	const tgrid::tchild& get_child(int at) const;
	void set_boddy(twidget* boddy);

	const tgrid::tchild* child_begin() const;

private:
	void tabbar_init(bool toggle, const std::string& definition, bool segment_switch);
	void multiline_init(bool toggle, const std::string& definition, int fixed_cols);
	void insert_reserve_widgets();
	tcontrol& create_child(const std::string& id, const std::string& label);
	int next_selectable(const int start_at);

	/** Inherited from tcontainer_. */
	void set_self_active(const bool active)
		{ state_ = active ? ENABLED : DISABLED; }

	/***** ***** ***** setters / getters for members ***** ****** *****/
	bool get_active() const { return state_ != DISABLED; }
	unsigned get_state() const { return state_; }

	void tabbar_erase_children();

	void tabbar_replacement_children();
	void multiline_replacement_children();

	void click(bool previous);
	void validate_start();
	void invalidate(const twidget& widget);

	bool did_pre_change(ttoggle_button& widget);
	void did_changed(ttoggle_button& widget);
	void did_click(twidget* widget, bool& handled, bool& halt);

	/** Inherited from tcontrol. */
	void impl_draw_background(
			  texture& frame_buffer
			, int x_offset
			, int y_offset) override;

	/** Inherited from tscroll_container. */
	tgrid* mini_create_content_grid() override;
	void mini_finalize_subclass() override;

	tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) override;
	void mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin) override;
	void validate_children_continuous() const override;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	void validate_visible_pre_change(const twidget& widget) const override;
private:
	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	tformula<int> unit_w_formula_;
	tformula<int> unit_h_formula_;
	tpoint unit_size_;
	int gap_;

	// true: it is multi-line report.
	// false: it is single-line report. same as tabbar
	bool multi_line_;

	int front_childs_;
	int back_childs_;

	//
	// tabbar member
	//

	// true: widget is toggle_button
	// false: widget is button
	bool toggle_;

	// true:  space for the previous/next is always reserved,
	// false: space for the previous/next is reserved according to current state.
	bool segment_switch_;

	// when segment_switch_, segment_childs_ is visible widget count at one group.
	// when !segment_switch_, segment_childs_ is visible widget count at this time.
	int segment_childs_;

	// valid only multiline report.
	int fixed_cols_;

	std::string definition_;
	int start_;

	tgrid2* grid2_; // it is content_grid_. use it for tgrid2's private function.
	tbutton* previous_;
	tbutton* next_;

	boost::function<void (treport&, tcontrol& widget)> callback_show_;
	boost::function<bool (treport&, ttoggle_button& widget, ttoggle_button* previous)> did_item_pre_change_;
	boost::function<void (treport&, ttoggle_button& widget)> did_item_changed_;
	boost::function<void (treport&, tbutton& widget)> did_item_click_;

	tpanel* boddy_;

	class tinternal_set_visible_lock
	{
	public:
		tinternal_set_visible_lock(treport& report)
			: report_(report)
			, original_(report.internal_set_visible_)
		{
			// false --> true or true --> true.
			report_.internal_set_visible_ = true;
		}
		~tinternal_set_visible_lock()
		{
			report_.internal_set_visible_ = original_;
		}

	private:
		treport& report_;
		bool original_;
	};
	bool internal_set_visible_;
};

} // namespace gui2

#endif

