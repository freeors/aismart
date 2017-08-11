/* $Id: scroll_label.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/report.hpp"

#include "gui/widgets/label.hpp"
#include "gui/auxiliary/widget_definition/report.hpp"
#include "gui/auxiliary/window_builder/report.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/dialog.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(report)

treport::treport(bool multi_line, bool toggle, const std::string& unit_definition, const std::string& unit_w, const std::string& unit_h, int gap, bool segment_switch, int fixed_cols)
	: tscroll_container(COUNT)
	, grid2_(nullptr)
	, state_(ENABLED)
	, multi_line_(multi_line)
	, unit_w_formula_(unit_w)
	, unit_h_formula_(unit_h)
	, unit_size_(nposm, nposm)
	, gap_(cfg_2_os_size(gap))
	, toggle_(true)
	, segment_switch_(false)
	, fixed_cols_(0)
	, definition_()
	, front_childs_(0)
	, back_childs_(0)
	, start_(0)
	, segment_childs_(0)
	, previous_(NULL)
	, next_(NULL)
	, boddy_(NULL)
	, internal_set_visible_(false)
{
	find_content_grid_ = true;

	// if (!multi_line) {
		// multi_line report maybe use in bottom navigation.
		unrestricted_drag_ = false;
	// }

	// calculate unit_w and unit_h
	game_logic::map_formula_callable variables;
	get_screen_size_variables(variables);

	unit_size_.x = unit_size_.y = 0;
	if (unit_w_formula_.has_formula2()) {
		unit_size_.x = unit_w_formula_(variables);
		if (unit_w_formula_.has_multi_or_noninteger_formula()) {
			unit_size_.x *= twidget::hdpi_scale;
		} else {
			unit_size_.x = cfg_2_os_size(unit_size_.x);
		}
	}
	if (unit_h_formula_.has_formula2()) {
		unit_size_.y = unit_h_formula_(variables);
		if (unit_h_formula_.has_multi_or_noninteger_formula()) {
			unit_size_.y *= twidget::hdpi_scale;
		} else {
			unit_size_.y = cfg_2_os_size(unit_size_.y);
		}
	}
	VALIDATE(unit_size_.x >= 0 && unit_size_.y >= 0, null_str);

	if (multi_line_) {
		multiline_init(toggle, unit_definition, fixed_cols);
	} else {
		tabbar_init(toggle, unit_definition, segment_switch);
	}
}

tgrid* treport::mini_create_content_grid()
{
	grid2_= new tgrid2(*this);
	return grid2_;
}

void treport::mini_finalize_subclass()
{
	VALIDATE(content_grid_, "content_grid_ must not be NULL!");
	if (!multi_line_) {
		VALIDATE(get_horizontal_scrollbar_mode() == always_invisible, "Don't support horizontal scrollbar in tabbar!");
	}
}

void treport::insert_reserve_widgets()
{
	if (!multi_line_ && !content_grid_->children_vsize()) {
		VALIDATE(previous_ && next_, null_str);
		grid2_->insert_child(*previous_, nposm);
		grid2_->insert_child(*next_, nposm);
	}
}

tcontrol& treport::insert_item(const std::string& id, const std::string& label, int at)
{
	tdisable_invalidate_layout_lock lock;

	VALIDATE(!definition_.empty(), "Must call tabbar_init or multiline_init before insert_child!");
	tcontrol& widget = create_child(id, label);

	insert_reserve_widgets();

	const int size = content_grid_->children_vsize();
	// insert_child is called when insert data item. insert front/back_childs_ item don't call it.
	VALIDATE(size >= front_childs_ + back_childs_, null_str);

	if (at != nposm) {
		at += front_childs_;
	} else {
		if (size >= front_childs_ + back_childs_) {
			at = size - back_childs_;
		}
	}

	grid2_->insert_child(widget, at);

	if (!multi_line_) {
		validate_start();
	}
	validate_children_continuous();

	return widget;
}

#define get_visible2(children, at)	(!((children)[at].flags_ & tgrid::USER_PRIVATE))
#define get_visible2_1(child)		(!((child).flags_ & tgrid::USER_PRIVATE))

#define set_visible2_1(child, visible)	((visible)? (child).flags_ &= ~tgrid::USER_PRIVATE: (child).flags_ |= tgrid::USER_PRIVATE)

void treport::erase_item(int at)
{
	int size = content_grid_->children_vsize();

	if (at != nposm) {
		VALIDATE(front_childs_ + at + back_childs_ < size, null_str);
		at += front_childs_;
	} else {
		VALIDATE(front_childs_ + back_childs_ < size, null_str);
		at = size - back_childs_ - 1;
	}

	// now at include front_childs_.
	const tgrid::tchild* children = content_grid_->children();
	const tgrid::tchild& child = children[at];

	bool original_visible = get_visible2_1(child);
	bool require_reselect = false;
	const bool auto_select = true;
	if (toggle_ && auto_select) {
		require_reselect = dynamic_cast<ttoggle_button*>(child.widget_)->get_value();
	}
	grid2_->erase_child(at);

	if (original_visible) {
		validate_start();
	}

	// if necessary, reselect.
	VALIDATE(content_grid_->children_vsize() == size - 1, null_str);
	size --;
	if (require_reselect) {
		int new_at = next_selectable(at);
		if (new_at != nposm) {
			select_item(new_at - front_childs_);
		}
	}
	validate_children_continuous();
}

int treport::next_selectable(const int start_at)
{
	VALIDATE(toggle_, null_str);

	// return new_at include front_childs_.
	// 1. [start_at ---> 
	// 2. <----start_at)
	int new_at = nposm;
	const tgrid::tchild* children = content_grid_->children();
	const int size = content_grid_->children_vsize();
	if (size > front_childs_ + back_childs_) {
		new_at = start_at;
		for (; new_at < size - back_childs_; new_at ++) {
			const tgrid::tchild& child = children[new_at];
			if (get_visible2_1(child) && dynamic_cast<tcontrol*>(child.widget_)->get_active()) {
				break;
			}
		}
		if (new_at == size - back_childs_) {
			for (new_at = start_at - 1; new_at >= front_childs_; new_at --) {
				const tgrid::tchild& child = children[new_at];
				if (get_visible2_1(child) && dynamic_cast<tcontrol*>(child.widget_)->get_active()) {
					break;
				}
			}
			if (new_at < front_childs_) {
				new_at = nposm;
			}
		}
	}
	return new_at;
}

void treport::clear()
{
	if (!multi_line_) {
		tabbar_erase_children();
	} else {
		grid2_->erase_children();
	}
}

void treport::hide_children()
{
	grid2_->hide_children();

	if (!multi_line_) {
		start_ = 0;
		segment_childs_ = 0;
	}
}

tpoint treport::tgrid2::get_multi_line_reference_size() const
{
	VALIDATE(report_.multi_line_, null_str);

	if (report_.unit_size_.x && report_.unit_size_.y) {
		return report_.unit_size_;
	}

	// if unit_size_.x/unit_size_.y is 0, from first vaisible item.
	tpoint result = report_.unit_size_;
	const int vsize = children_vsize();
	for (int n = 0; n < vsize; n ++) {
		tchild& child = children_[n];
		if (!get_visible2_1(child)) {
			continue;
		}
		// consider first visible item only.
		tpoint size = child.widget_->get_best_size();
		if (!result.x) {
			result.x = size.x;
		}
		if (!result.y) {
			result.y = size.y;
		}	
		break;
	}
	return result;
}

tpoint treport::tgrid2::calculate_best_size() const
{
	const int unit_w = report_.unit_size_.x;
	const int unit_h = report_.unit_size_.y;
	const int fixed_cols = report_.fixed_cols_;

	tpoint result(0, 0);
	const int vsize = children_vsize();
	if (!report_.multi_line_) {
		if (report_.segment_switch_) {
			return tpoint(0, unit_h);
		}
		int max_height = 0;
		tpoint size(0, 0);
		for (int n = report_.front_childs_; n < vsize - report_.back_childs_; n ++) {
			tchild& child = children_[n];
			if (!get_visible2_1(child)) {
				continue;
			}
			if (!unit_w || !unit_h) {
				size = child.widget_->get_best_size();
			}
			if (unit_w) {
				size.x = unit_w;
			}
			if (unit_h) {
				size.y = unit_h;
			}
			result.x += size.x;
			if (size.y > max_height) {
				max_height = size.y;
			}
		}
		result.y = max_height;
		return result;
	}

	if (!fixed_cols) {
		return tpoint(0, 0); 
	}

	int vsize2 = 0;
	for (int n = 0; n < vsize; n ++) {
		tchild& child = children_[n];
		if (!get_visible2_1(child)) {
			continue;
		}
		vsize2 ++;
	}

	int quotient = vsize2 / fixed_cols;
	int remainder = vsize2 % fixed_cols;
	int rows = quotient + (remainder? 1: 0);

	tpoint reference_size = get_multi_line_reference_size();
	return tpoint(fixed_cols * reference_size.x + (fixed_cols - 1) * report_.gap_, rows * reference_size.y + (rows - 1) * report_.gap_);
}

void treport::tgrid2::place(const tpoint& origin, const tpoint& size)
{
	VALIDATE(size.x > 0 && size.y > 0, null_str);
	tinternal_set_visible_lock lock(report_);

	tdisable_invalidate_layout_lock lock2;

	// both tabbar and multi_line report, widget in content_grid_ is placed by myself.
	// 1. set desire origin and size.
	report_.content_grid_->twidget::place(origin, size);
	// 2. place myself

	if (!report_.multi_line_) {
		report_.tabbar_replacement_children();

	} else {
		report_.multiline_replacement_children();
	}
}

void treport::impl_draw_background(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	if (!multi_line_) {
		grid2_->update_last_draw_end();
	}
	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);
}

tpoint treport::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	return content_size;
}

void treport::mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_grid_origin)
{
	content_grid_->set_origin(content_grid_origin);

	if (!multi_line_ && boddy_) {
		const twidget* widget = cursel();
		if (widget && widget->get_visible() == twidget::VISIBLE) {
			const int x_offset = content_grid_origin.x - origin.x;
			int left = widget->get_x() >= origin.x? widget->get_x(): origin.x;
			int right = widget->get_x() + (int)widget->get_width() >= origin.x? widget->get_x() + widget->get_width(): origin.x;
			boddy_->set_hole_variable(left, right);
		} else {
			boddy_->set_hole_variable(0, 0);
		}
	}
}

void treport::validate_children_continuous() const
{
	const tgrid::tchild* children = content_grid_->children();
	const int size = content_grid_->children_vsize();
	// size had got rid of stuff_widdget_.

	int next_at = 0;
	for (int at = 0; at < size; at ++) {
		tcontrol& widget = *dynamic_cast<tcontrol*>(children[at].widget_);
		if (at < front_childs_) {
			VALIDATE(widget.at_ == nposm, null_str);
		} else if (at < size - back_childs_) {
			VALIDATE(widget.at_ == next_at, null_str);
			next_at ++;
		} else {
			VALIDATE(widget.at_ == nposm, null_str);
		}
	}
}

const std::string& treport::get_control_type() const
{
	static const std::string type = "report";
	return type;
}

// decrement start_ if necessary.
void treport::validate_start()
{
	if (!start_) {
		return;
	}
	gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;

	if (segment_switch_) {
		// to segment_switch_, start_ must be integer times of segment_childs_
		start_ -= start_ % segment_childs_;
	}
	int n, tmp;
	for (n = front_childs_, tmp = 0; n < childs && tmp <= start_; n ++) {
		if (get_visible2(children, n)) {
			tmp ++;
		}
	}
	while (start_ && tmp <= start_) {
		if (segment_switch_) {
			start_ -= segment_childs_;
		} else {
			start_ --;
		}
	}
}

void treport::click(bool previous)
{
	VALIDATE(!multi_line_ && segment_switch_, null_str);

	if (previous) {
		start_ -= segment_childs_;
	} else {
		start_ += segment_childs_;
	}

	// it must be same-size, to save time, don't calcuate linked_group.
	invalidate_layout(nullptr);
}

void treport::tabbar_init(bool toggle, const std::string& definition, bool segment_switch)
{
	// VALIDATE(!content_grid_->children_vsize(), "Duplicate call!");
	VALIDATE(!multi_line_, "It is multi-line report, cannot call tabbar_init!");

	std::string type = toggle? "toggle_button": "button";
	VALIDATE(is_control_definition(type, definition), "Muset set a valid definition!");

	front_childs_ = 1; // 1: previous
	back_childs_ = 1; // 1: next

	toggle_ = toggle;
	definition_ = definition;
	start_ = 0;
	segment_childs_ = 0;
	segment_switch_ = segment_switch;

	VALIDATE(!segment_switch_ || (unit_size_.x > 0 && unit_size_.y > 0), "Must valid width and height!");

	// previous arrow
	previous_ = create_blits_button("previous");
	previous_->set_layout_size(tpoint(unit_size_.x, unit_size_.y));
	tformula_blit blit("misc/arrow-left.png", null_str, null_str, str_cast(unit_size_.x), str_cast(unit_size_.y));
	previous_->set_blits(blit);
	connect_signal_mouse_left_click(
		*previous_
		, boost::bind(
			&treport::click
			, this
			, true));

	// next arrow
	next_ = create_blits_button("next");
	next_->set_layout_size(tpoint(unit_size_.x, unit_size_.y));
	blit = tformula_blit("misc/arrow-right.png", null_str, null_str, str_cast(unit_size_.x), str_cast(unit_size_.y));
	next_->set_blits(blit);
	connect_signal_mouse_left_click(
		*next_
		, boost::bind(
			&treport::click
			, this
			, false));
}

void treport::multiline_init(bool toggle, const std::string& definition, int fixed_cols)
{
	// it is maybe call after one place successfully.
	// VALIDATE(!content_grid_->children_vsize(), "Duplicate call!");
	VALIDATE(multi_line_, "It is tabbar report, cannot call multiline_inlit!");

	std::string type = toggle? "toggle_button": "button";
	VALIDATE(is_control_definition(type, definition), "Muset set a valid definition!");

	toggle_ = toggle;
	definition_ = definition;
	start_ = 0;
	segment_childs_ = 0;

	VALIDATE(fixed_cols >= 0, null_str);
	fixed_cols_ = fixed_cols;
}

bool treport::did_pre_change(ttoggle_button& widget)
{
	ttoggle_button* previous = cursel();
	// if no selected item existed, previous maybe nullptr.
	
	if (did_item_pre_change_) {
		if (!did_item_pre_change_(*this, widget, previous)) {
			return false;
		}
	}
	return true;
}

void treport::did_changed(ttoggle_button& widget)
{
	// only set true pass.
	if (!widget.get_value()) {
		return;
	}

	select_item(widget);
	if (did_item_changed_) {
		did_item_changed_(*this, widget);
	}
}

void treport::did_click(twidget* widget, bool& handled, bool& halt)
{
	if (did_item_click_) {
		did_item_click_(*this, *dynamic_cast<tbutton*>(widget));
	}

	handled = true;
	// app maybe connect self function to every tbutton. so don't set halt = true.
	// halt = true;
}

tcontrol& treport::create_child(const std::string& id, const std::string& label)
{
	tcontrol* widget;
	if (toggle_) {
		ttoggle_button* widget2 = create_toggle_button(id, definition_, label);
		widget2->set_radio(true);
		widget2->set_did_state_pre_change(boost::bind(&treport::did_pre_change, this, _1));
		widget2->set_did_state_changed(boost::bind(&treport::did_changed, this, _1));
		widget = widget2;

	} else {
		widget = create_button(id, definition_, label);
		connect_signal_mouse_left_click(
			*widget
			, boost::bind(
				&treport::did_click
				, this
				, widget
				, _3, _4));
	}
	widget->set_text_font_size(text_font_size_);
	widget->set_text_color_tpl(text_color_tpl_);

	return *widget;
}

void treport::tabbar_erase_children()
{
	// user call it to erase children. 
	// window don't call it. it erase children in tgrid::~tgrid.
	tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();

	int i;
	if (false) {
		// back childs
		for (i = childs - 1; i > childs - back_childs_ - 1; i --) {
			children[i].widget_->set_parent(NULL);
			children[i].widget_ = NULL;
			grid2_->erase_child(i);
		}
		next_ = NULL;
	}

	// content childs
	for (i = childs - back_childs_ - 1; i > 0; i --) {
		grid2_->erase_child(i);
	}

	if (false) {
		// front childs
		children[0].widget_->set_parent(NULL);
		children[0].widget_ = NULL;
		grid2_->erase_child(0);
		previous_ = NULL;
	}

	start_ = 0;
	segment_childs_ = 0;
}

void treport::tabbar_replacement_children()
{
	const tgrid::tchild* children = content_grid()->children();
	const int vsize = content_grid_->children_vsize();

	const int grid_width = content_grid_->get_width();
	const int grid_height = content_grid_->get_height();
	VALIDATE(grid_width && grid_height, null_str);

	if (!vsize) {
		return;
	}
	
	int unit_w = unit_size_.x;
	if (segment_switch_) {
		if (!segment_childs_) {
			segment_childs_ = (grid_width + gap_) / (unit_size_.x + gap_) - 2;
			VALIDATE(segment_childs_ > 0, "It is segment tabbar, accommodatable widgets must be large than 2!");
		}
	} else {
		segment_childs_ = 0;
	}

	int last_n = nposm, last_width = 0;
	bool require_next = false;

	int n = front_childs_;
	for (int tmp = 0; tmp < start_; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		// these widget not in current group, as if user require visilbe, but invisible.
		widget->set_visible(twidget::INVISIBLE);

		if (get_visible2(children, n)) {
			tmp ++;
		}
	}

	twidget::tvisible previous_visible = twidget::INVISIBLE;
	if (segment_switch_) {
		previous_visible = start_? twidget::VISIBLE: twidget::HIDDEN;
	}

	children[0].widget_->set_visible(previous_visible);
	int additive_width = previous_visible != twidget::INVISIBLE? children[0].widget_->get_best_size().x: 0;
	int additive_child = 0;

	int end = vsize - back_childs_;
	for (; n < end; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		if (!get_visible2(children, n)) {
			// USER_PRIVATE(=1): user force it invisible.
			widget->set_visible(twidget::INVISIBLE);
			continue;
		}

		if (!unit_size_.x) {
			unit_w = widget->get_best_size().x;
		}
		if (require_next || (segment_switch_ && additive_child == segment_childs_)) {
			widget->set_visible(twidget::INVISIBLE);
			if (!require_next) {
				require_next = true;
			}
			continue;
		}
		widget->set_visible(twidget::VISIBLE);
		if (callback_show_) {
			callback_show_(*this, *dynamic_cast<tcontrol*>(child.widget_));
		}

		if (!segment_switch_) {
			additive_width += unit_w + gap_;
			last_n = n;
			last_width = unit_w;
			segment_childs_ ++;
		} else {
			additive_child ++;
		}
	}

	int spacer_width = 0;
	if (require_next) {
		VALIDATE(segment_switch_, null_str);
		if (last_n != nposm) {
			children[last_n].widget_->set_visible(twidget::INVISIBLE);
			segment_childs_ --;
		}
	}

	next_->set_visible(require_next? twidget::VISIBLE: twidget::INVISIBLE);

	grid2_->replacement_children();

	if (boddy_) {
		twidget* widget = cursel();
		if (widget && widget->get_visible() == twidget::VISIBLE) {
			boddy_->set_hole_variable(widget->get_x(), widget->get_x() + widget->get_width());
		} else {
			boddy_->set_hole_variable(0, 0);
		}
	}
}

void treport::multiline_replacement_children()
{
	const tgrid::tchild* children = content_grid()->children();
	const int vsize = content_grid_->children_vsize();

	for (int n = 0; n < vsize; n ++) {
		const tgrid::tchild& child = children[n];
		twidget* widget = child.widget_;

		widget->set_visible(get_visible2_1(child)? twidget::VISIBLE: twidget::INVISIBLE);
	}
	grid2_->replacement_children();
}

const tgrid::tchild* treport::child_begin() const
{
	return content_grid_->children() + front_childs_;
}

int treport::items() const
{
	return content_grid_->children_vsize() - front_childs_ - back_childs_;
}

void treport::set_boddy(twidget* boddy) 
{ 
	boddy_ = dynamic_cast<tpanel*>(boddy);
}

void treport::validate_visible_pre_change(const twidget& widget) const
{
	if (&widget == this) {
		return;
	}
	if (internal_set_visible_) {
		return;
	}
	VALIDATE(false, "use set_item_visible to set item's visible.");
}

void treport::set_item_visible(int at, bool visible)
{
	gui2::tgrid::tchild* children = content_grid_->children();
	const int childs = content_grid_->children_vsize() - front_childs_ - back_childs_;
	VALIDATE(at >= 0 && at < childs, null_str);

	at += front_childs_;
	tgrid::tchild& child = children[at];
	if (get_visible2_1(child) == visible) {
		return;
	}
	set_visible2_1(child, visible);
	validate_start();

	const bool auto_select = true;
	bool require_reselect = false;
	if (toggle_ && auto_select) {
		require_reselect = dynamic_cast<ttoggle_button*>(child.widget_)->get_value();
	}
	if (require_reselect) {
		int new_at = next_selectable(at);
		if (new_at != nposm) {
			select_item(new_at - front_childs_);
		}
	}

	// it must be same-size, to save time, don't calcuate linked_group.
	invalidate_layout(nullptr);
}

bool treport::get_item_visible(int at) const
{
	const tgrid::tchild* children = content_grid_->children();
	return get_visible2(children, at + front_childs_);
}

void treport::set_item_visible(const tcontrol& widget, bool visible)
{
	gui2::tgrid::tchild* children = content_grid()->children();
	const int childs = content_grid()->children_vsize() - back_childs_;
	for (int n = front_childs_; n < childs; n ++) {
		tgrid::tchild& child = children[n];
		if (child.widget_ != &widget) {
			continue;
		}
		if (get_visible2_1(child) == visible) {
			return;
		}
		set_item_visible(n - front_childs_, visible);
		return;
	}

	VALIDATE(false, null_str);
}

bool treport::get_item_visible(const tcontrol& widget) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	const int childs = content_grid()->children_vsize() - back_childs_;
	for (int n = front_childs_; n < childs; n ++) {
		const tgrid::tchild& child = children[n];
		if (child.widget_ != &widget) {
			continue;
		}
		return get_visible2_1(child);
	}

	VALIDATE(false, null_str);
	return false;
}

void treport::invalidate(const twidget& widget)
{
	twindow* window = get_window();
	if (!window->layouted()) {
		return;
	}

	bool require_layout_window = unit_size_.y? false: true;
	if (require_layout_window) {
		// VALIDATE(!multi_line_, null_str);
		int height = widget.get_best_size().y;
		if (height <= (int)content_->get_height()) {
			require_layout_window = false;
		}	
	}
	if (require_layout_window) {
		get_window()->invalidate_layout(nullptr);
	} else {
		// it must be same-size, to save time, don't calcuate linked_group.
		invalidate_layout(nullptr);
	}
}

int treport::get_at2(const void* cookie2) const
{
	const size_t cookie = reinterpret_cast<size_t>(cookie2);
	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize() - back_childs_;
	for (int i = front_childs_; i < childs; i ++) {
		if (children[i].widget_->cookie() == cookie) {
			return i - front_childs_;
		}
	}
	return nposm;
}

const tgrid::tchild& treport::get_child(int at) const
{
	const gui2::tgrid::tchild* children = content_grid()->children();
	return children[front_childs_ + at];
}

tcontrol& treport::item(int at) const
{
	const gui2::tgrid::tchild* children = content_grid_->children();
	const int size = content_grid_->children_vsize();
	if (at == nposm) {
		at = size - front_childs_ - back_childs_ - 1;
	}
	VALIDATE(front_childs_ + at + back_childs_ < size, null_str);
	return *dynamic_cast<tcontrol*>(children[front_childs_ + at].widget_);
}

void treport::select_item(int at)
{
	VALIDATE(toggle_, null_str);
	const tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();
	if (childs < front_childs_ + at + back_childs_) {
		return;
	}
	select_item(*children[front_childs_ + at].widget_);
}

// to toggle_button's report, must exist one selected in any time.
void treport::select_item(twidget& widget)
{
	VALIDATE(toggle_, null_str);
	
	// first call set_value(true). because did_pre_change require cursel() to get previous.
	ttoggle_button* widget2 = dynamic_cast<ttoggle_button*>(&widget);
	if (!widget2->get_value()) {
		widget2->set_value(true);
	}

	const gui2::tgrid::tchild* children = content_grid()->children();
	int childs = content_grid()->children_vsize();
	for (int i = front_childs_; i < childs - back_childs_; i ++) {
		const tgrid::tchild& child = children[i];
		// as if invisible, deselect.
/*
		if (!get_visible2_1(child)) {
			continue;
		}
*/
		ttoggle_button* that = dynamic_cast<ttoggle_button*>(child.widget_);
		if (that != &widget && that->get_value()) {
			that->set_value(false);
		}
	}

	if (boddy_) {
		boddy_->set_hole_variable(widget2->get_x(), widget2->get_x() + widget2->get_width());
	}
}

ttoggle_button* treport::cursel() const
{
	if (!toggle_) {
		return nullptr;
	}

	const gui2::tgrid::tchild* children = content_grid()->children();
	const int childs = content_grid_->children_vsize();
	for (int i = front_childs_; i < childs - back_childs_; i ++) {
		const tgrid::tchild& child = children[i];
		if (!get_visible2_1(child)) {
			continue;
		}
		ttoggle_button* that = dynamic_cast<ttoggle_button*>(child.widget_);
		if (that->get_value()) {
			return that;
		}
	}

	// as if must exist one selected in any time, but maybe no select when first create more.
	return nullptr;
}

void treport::tgrid2::layout_init(bool linked_group_only)
{
	// normal flow, do nothing.
	// twidget::layout_init(linked_group_only);
}

void treport::tgrid2::report_create_stuff(int unit_w, int unit_h, int gap, bool extendable, int fixed_cols)
{
	static const std::string report_stuff_id = "_stuff";

	VALIDATE(!valid_stuff_size_ && stuff_widget_.empty(), "Must no stuff widget in report!");
	// VALIDATE(rows_ == 1, "Must one row in report!");

	std::stringstream ss;

	int cols = 1;
	if (extendable) {
		VALIDATE(unit_w || fixed_cols, "Multiline only support fixed size!");
		/*
		 * width >= n * unit_w + (n - 1) * gap_w;
		 * width >= n * unit_w + n * gap_w - gap_w;
		 * width >= n * (unit_w + gap_w) - gap_w;
		 * n < width + gap_w / (unit_w + gap_w);
		*/
		if (!fixed_cols) {
			cols = (w_ + gap) / (unit_w + gap);
		} else {
			cols = fixed_cols;
		}
	}

	for (int n = 0; n < cols; n ++) {
		ss.str("");
		ss << report_stuff_id << n;
		tspacer* spacer = create_spacer(ss.str());
		insert_child(*spacer, nposm);
		stuff_widget_.push_back(spacer);
	}

	valid_stuff_size_ = cols;
}

void treport::tgrid2::calculate_grid_params(int unit_w, int unit_h, int gap, bool multiline, int fixed_cols)
{
	if (stuff_widget_.empty()) {
		if (multiline) {
			// multi-report must use fixed size.
			// in order to calculate cols, first require know w_, h_
			// VALIDATE(w_ >= (unsigned)(unit_w + gap + unit_w), "Width of report must not less than 2*unit_w + gap_w!");
			VALIDATE(w_ >= unit_w + gap, "Width of report must not less than unit_w + gap_w!");
		}
		report_.insert_reserve_widgets();
		report_create_stuff(unit_w, unit_h, gap, multiline, fixed_cols);
	}

	// cols_, rows_, row_height_, col_width_
	const int vsize = children_vsize_ - (int)stuff_widget_.size();

	if (!multiline) {
		rows_ = 1;
		row_grow_factor_.resize(rows_, 0);
		row_height_.clear();
		row_height_.resize(rows_, unit_h);

		cols_ = vsize + 1;
		// last is stuff. it's grow factor is 1, fill when replacement.
		col_grow_factor_.resize(cols_, 0);
		// if unit_w == 0, col_width_ will update when replacement.
		col_width_.resize(cols_, unit_w);

	} else {
		const int cols = fixed_cols? fixed_cols: (w_ + gap) / (unit_w + gap);
		int quotient = vsize / cols;
		int remainder = vsize % cols;

		VALIDATE((int)cols == stuff_widget_.size(), "Must create equal size stuff!");

		rows_ = quotient + (remainder? 1: 0);
		cols_ = cols;
		row_grow_factor_.resize(rows_, 0);
		row_grow_factor_.resize(rows_, 0);

		row_height_.clear();
		row_height_.resize(rows_, unit_h);
		col_width_.clear();
		col_width_.resize(cols_, unit_w);

		if (remainder) {
			valid_stuff_size_ = cols - remainder;
		} else {
			valid_stuff_size_ = 0;
		}
	}
}

void treport::tgrid2::replacement_children()
{
	int unit_w = report_.unit_size_.x; // if multi_line_ && fixed_cols_, it maybe more.
	int unit_h = report_.unit_size_.y;
	const int gap = report_.gap_;
	const tspacer& content = *report_.content_;

	if (report_.multi_line_ && (!unit_w || !unit_h)) {
		tpoint result = get_multi_line_reference_size();
		unit_w = result.x;
		unit_h = result.y;
	}
	calculate_grid_params(unit_w, unit_h, gap, report_.multi_line_, report_.fixed_cols_);

	const int max_cols = stuff_widget_.size();
	int n = 0, row = 0, col = 0;
	int horizontal_advance = gap;
	int end;
	if (report_.multi_line_) {
		end = children_vsize_ - (max_cols - valid_stuff_size_);
		VALIDATE(!(end % max_cols), null_str);
		if (report_.fixed_cols_) {
			unit_w = (w_ - (report_.fixed_cols_ - 1) * gap) / report_.fixed_cols_; 
		} else {
			int cols = 1;
			while ((cols - 1) * gap + cols * unit_w < w_) {
				cols ++;
			}
			cols --;
			int bonus = w_ - ((cols - 1) * gap + cols * unit_w);
			horizontal_advance += bonus / (cols - 1);
		}
	} else {
		end = children_vsize_ - valid_stuff_size_;
	}

	tpoint origin(x_, y_);
	tpoint size(unit_w, unit_h);
	int multiline_max_width = 0;
	for (n = 0; n < end; n ++) {
		tchild& child = children_[n];
		twidget* widget2 = child.widget_;
		twidget::tvisible visible = widget2->get_visible();
		if (visible == twidget::INVISIBLE) {
			continue;
		}

		tpoint origin2 = widget2->get_origin();
		tpoint size2 = widget2->get_size();
		if (report_.multi_line_) {
			if (col) {
				child.flags_ |= tgrid::BORDER_LEFT;
				origin.x += horizontal_advance;
			}
			if (row && !col) {
				child.flags_ |= tgrid::BORDER_TOP;
				origin.y += gap;
			}
		} else if (col) {
			child.flags_ |= tgrid::BORDER_LEFT;
			origin.x += horizontal_advance;
		}
		if (child.flags_) {
			child.border_size_ = gap;
		}

		child.flags_ |= VERTICAL_ALIGN_CENTER | HORIZONTAL_ALIGN_CENTER;

		if (!unit_w) {
			size = widget2->get_best_size();
			// widget2->set_layout_size(size);
			col_width_[col] = size.x;
			if (size.y > static_cast<int>(row_height_[0])) {
				row_height_[0] = size.y;
			}
		}
		if (w_ && visible == twidget::VISIBLE) {
			if (origin2 != origin || size2 != size) {
				if (size2 == size) {
					widget2->twidget::place(origin, size);
				} else {
					widget2->place(origin, size);
				}
			} else if (!report_.multi_line_) {
				widget2->set_visible_area(get_rect());
			}
		}
		origin.x += size.x;

		col ++;
		if (report_.multi_line_) {
			if (origin.x - x_ > multiline_max_width) {
				multiline_max_width = origin.x - x_;
			}
			if (col == max_cols) {
				origin.x = x_;
				origin.y += unit_h;
				row ++;
				col = 0;
			}
		}
	}

	if (report_.multi_line_) {
		int w = content.get_width();
		if (multiline_max_width > w) {
			w = multiline_max_width;
		}
		int h = content.get_height();
		if (origin.y - y_ > h) {
			h = origin.y - y_;
		}
		w_ = w;
		h_ = h;

	} else {
		children_[children_vsize_ - 1].flags_ = VERTICAL_ALIGN_EDGE | HORIZONTAL_ALIGN_EDGE;
		// place stuff. stuff is always visible.
		if (origin.x < last_draw_end_.x) {
			size.x = last_draw_end_.x - origin.x;
		} else {
			size.x = 0;
		}
		tspacer* stuff = stuff_widget_[0];
		col_grow_factor_[col_grow_factor_.size() - 1] = 1;
		if (w_) {
			col_width_[col] = size.x;

			size.y = row_height_[0];
			stuff->set_layout_size(size);
		}
		stuff->place(origin, size);
		stuff->set_visible_area(get_rect());

		if (!report_.segment_switch_) {
			w_ = content.get_width();
			if (origin.x - x_ > (int)w_) {
				w_ = origin.x - x_;
			}
		}
	}

	// remember it, speed up get_best_size.
	set_layout_size(tpoint(w_, h_));
}

int treport::tgrid2::insert_child(twidget& widget, int at)
{
	// make sure the new child is valid before deferring
	widget.set_parent(this);
	const int stuff_size = (int)stuff_widget_.size();
	if (at == nposm || at > children_vsize_ - stuff_size) {
		at = children_vsize_ - stuff_size;
	}

	// below memmove require children_size_ are large than children_vsize_!
	resize_children(children_vsize_ + 1);

	if (children_vsize_ - at) {
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	children_vsize_ ++;

	if (children_vsize_ > report_.front_childs_ + report_.back_childs_ + stuff_size) {
		dynamic_cast<tcontrol*>(children_[at].widget_)->at_ = at - report_.front_childs_;
		for (int at2 = at + 1; at2 < children_vsize_ - report_.back_childs_ - stuff_size; at2 ++) {
			tcontrol* widget = dynamic_cast<tcontrol*>(children_[at2].widget_);
			widget->at_ ++;
		}
	}

	report_.invalidate(widget);

	return at;
}

void treport::tgrid2::erase_child(int at)
{
	if (stuff_widget_.empty()) {
		return;
	}

	const int stuff_size = (int)stuff_widget_.size();
	if (at == nposm || at >= children_vsize_ - stuff_size) {
		return;
	}
	delete children_[at].widget_;
	children_[at].widget_ = NULL;
	if (at < children_vsize_ - 1) {
		memcpy(&(children_[at]), &(children_[at + 1]), (children_vsize_ - at - 1) * sizeof(tchild));
	}
	children_[children_vsize_ - 1].widget_ = NULL;
	children_vsize_ --;

	if (children_vsize_ > report_.front_childs_ + report_.back_childs_ + stuff_size) {
		for (int at2 = at; at2 < children_vsize_ - report_.back_childs_ - stuff_size; at2 ++) {
			tcontrol* widget = dynamic_cast<tcontrol*>(children_[at2].widget_);
			widget->at_ --;
		}
	}

	// it must be same-size, to save time, don't calcuate linked_group.
	report_.invalidate_layout(nullptr);
}

void treport::tgrid2::erase_children()
{
	if (stuff_widget_.empty()) {
		return;
	}

	const int stuff_size = (int)stuff_widget_.size();
	int n = 0;
	for (; n < children_vsize_ - stuff_size; n ++) {
		delete children_[n].widget_;
		children_[n].widget_ = NULL;
	}
	for (int n2 = 0; n < children_vsize_; n ++, n2 ++) {
		children_[n2] = children_[n];
	}
	children_vsize_ = stuff_size;

	rows_ = 1;
	cols_ = children_vsize_;
	col_grow_factor_.resize(cols_);
	if (!report_.multi_line_) {
		col_width_.resize(cols_);
	}

	// it must be same-size, to save time, don't calcuate linked_group.
	report_.invalidate_layout(nullptr);
}

void treport::tgrid2::hide_children()
{
	if (stuff_widget_.empty()) {
		return;
	}

	const int stuff_size = (int)stuff_widget_.size();
	for (int n = 0; n < children_vsize_ - stuff_size; n ++) {
		tchild& child = children_[n];
		set_visible2_1(child, false);
	}
	
	// it must be same-size, to save time, don't calcuate linked_group.
	report_.invalidate_layout(nullptr);
}

void treport::tgrid2::update_last_draw_end()
{
	if (stuff_widget_.empty()) {
		return;
	}

	VALIDATE(stuff_widget_.size() == 1, null_str);
	last_draw_end_ = stuff_widget_[0]->get_origin();

	const tspacer* content = report_.content_spacer();
	if (last_draw_end_.x > content->get_x() + (int)content->get_width()) {
		// ensure not than more content_ rect.
		last_draw_end_.x = content->get_x() + content->get_width();
	}
}

} // namespace gui2

