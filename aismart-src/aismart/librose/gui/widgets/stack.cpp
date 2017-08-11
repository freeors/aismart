/* $Id: stack.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/stack.hpp"

#include "gui/auxiliary/widget_definition/stack.hpp"
#include "gui/auxiliary/window_builder/stack.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <numeric>

namespace gui2 {

REGISTER_WIDGET(stack)

tstack::tstack()
	: tcontainer_(1)
	, grid_(*this)
	, mode_(pip)
	// vertical/horizontal
	, center_close_(true)
	, upward_(true)
	, splitter_bar_(nullptr)
	, first_coordinate_(construct_null_coordinate())
	, diff_(0, 0)
	, std_top_rect_(null_rect)
	, max_top_movable_(nposm)
	, std_bottom_rect_(null_rect)
	, top_grid_(nullptr)
	, bottom_grid_(nullptr)
{
	set_container_grid(grid_);
	grid_.set_parent(this);
}

void tstack::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> layers;
	std::vector<tgrid3*> noauto_draw_children_layers;
	tgrid::tchild* children = grid_.children();
	const int childs = grid_.children_vsize();

	std::vector<std::vector<twidget*> >& dirty_list = caller.dirty_list();

	if (mode_ == pip) {
		for (int n = 0; n < childs; ++ n) {
			size_t dirty_size = dirty_list.size();

			// tgrid <==> [layer]
			tgrid3& grid = *(dynamic_cast<tgrid3*>(children[n].widget_));
			if (grid.get_visible() != twidget::VISIBLE) {
				continue;
			}
			std::vector<twidget*> child_call_stack = call_stack;
			if (n > 0) {
				child_call_stack.insert(child_call_stack.end(), layers.begin(), layers.end());
			}
			grid.populate_dirty_list(caller, child_call_stack);
	
			if (dirty_list.size() != dirty_size) {
				if (dirty_list.back().back() != &grid) {
					// has children dirty it. it shouldn't draw children. 
					grid.set_auto_draw_children(false);
					noauto_draw_children_layers.push_back(&grid);

				} else if (!noauto_draw_children_layers.empty()) {
					for (std::vector<tgrid3*>::const_iterator it = noauto_draw_children_layers.begin(); it != noauto_draw_children_layers.end(); ++ it) {
						tgrid3* grid3 = *it;
						grid3->set_auto_draw_children(true);
					}
					noauto_draw_children_layers.clear();
				}
			}
			layers.push_back(&grid);
		}
	} else {
		// radio/vertical/horizontal
		for (int n = 0; n < childs; ++ n) {
			// tgrid <==> [layer]
			tgrid3& grid = *(dynamic_cast<tgrid3*>(children[n].widget_));
			if (grid.get_visible() != twidget::VISIBLE) {
				continue;
			}
			std::vector<twidget*> child_call_stack = call_stack;
			grid.populate_dirty_list(caller, child_call_stack);
		}
	}
}

void tstack::layout_children()
{
	tgrid::tchild* children = grid_.children();
	const int childs = grid_.children_vsize();

	if (mode_ == radio) {
		for (int n = 0; n < childs; n ++) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() == twidget::VISIBLE) {
				grid->layout_children();
			}
		}

	} else {
		for (int n = childs - 1; n >= 0; n --) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() == twidget::VISIBLE) {
				grid->layout_children();
			}
		}
	}
}

void tstack::dirty_under_rect(const SDL_Rect& clip)
{
	tgrid::tchild* children = grid_.children();
	int childs = grid_.children_vsize();

	if (mode_ == pip) {
		for (int n = childs - 1; n >= 0; n --) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() != twidget::VISIBLE) {
				continue;
			}
			grid->dirty_under_rect(clip);
			// BUG!! require fixed in the future. now only dirty top grid.
			break;
		}
	} else {
		// radio/vertical/horizontal
		for (int n = 0; n < childs; n ++) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() != twidget::VISIBLE) {
				continue;
			}
			grid->dirty_under_rect(clip);
		}
	}
}

twidget* tstack::find_at(const tpoint& coordinate, const bool must_be_active)
{ 
	tgrid::tchild* children = grid_.children();
	int childs = grid_.children_vsize();

	if (mode_ == pip) {
		// only find topest grid.
		for (int n = childs - 1; n >= 0; n --) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() != twidget::VISIBLE) {
				continue;
			}
			return grid->find_at(coordinate, must_be_active);
		}
	} else {
		// radio/vertical/horizontal
		for (int n = 0; n < childs; n ++) {
			tgrid* grid = dynamic_cast<tgrid*>(children[n].widget_);
			if (grid->get_visible() != twidget::VISIBLE) {
				continue;
			}
			twidget* ret = grid->find_at(coordinate, must_be_active);
			if (ret) {
				return ret;
			}
		}
	}
	return NULL;
}

void tstack::tgrid2::stack_init()
{
	VALIDATE(!children_vsize_, "Must no child in report!");
	VALIDATE(!rows_ && !cols_, "Must no row and col in report!");

	if (stack_.mode_ != horizontal) {
		cols_ = 1; // Only one col in these mode.
		col_grow_factor_.push_back(0);
		col_width_.push_back(0);
	} else {
		rows_ = 1; // Only one row in horizontal mode.
		row_grow_factor_.push_back(0);
		row_height_.push_back(0);
	}
}

void tstack::tgrid2::stack_insert_child(twidget& widget, int at, uint32_t factor)
{
	// make sure the new child is valid before deferring
	widget.set_parent(this);
	if (at == nposm || at > children_vsize_) {
		at = children_vsize_;
	}

	// below memmove require children_size_ are large than children_vsize_!
	// resize_children require large than exactly rows_*cols_. because children_vsize_ maybe equal it.

	// i think, all grow factor is 0. except last stuff.
	if (stack_.mode_ != horizontal) {
		row_grow_factor_.push_back(factor);
		rows_ ++;
		row_height_.push_back(0);
	} else {
		col_grow_factor_.push_back(factor);
		cols_ ++;
		col_width_.push_back(0);
	}

	resize_children((rows_ * cols_) + 1);
	if (children_vsize_ - at) {
		memmove(&(children_[at + 1]), &(children_[at]), (children_vsize_ - at) * sizeof(tchild));
	}

	children_[at].widget_ = &widget;
	children_vsize_ ++;

	// replacement
	children_[at].flags_ = VERTICAL_ALIGN_EDGE | HORIZONTAL_ALIGN_EDGE;
}

tpoint tstack::tgrid2::calculate_best_size() const
{
	/*
	 * The best size is the combination of the greatest width and greatest
	 * height.
	 */
	const int childs = children_vsize_;
	tpoint result(0, 0);
	if (stack_.mode_ == pip || stack_.mode_ == vertical) {
		for (int i = 0; i < childs; ++ i) {
			const tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			if (grid->get_visible() == INVISIBLE) {
				continue;
			}

			const tpoint best_size = grid->get_best_size();
			if (best_size.x > result.x) {
				result.x = best_size.x;
			}

			row_height_[i] = best_size.y;
			if (stack_.mode_ != vertical) {
				if (best_size.y > result.y) {
					result.y = best_size.y;
				}
			} else {
				result.y += best_size.y;
			}
		}
		col_width_[0] = result.x;

	} else if (stack_.mode_ == radio) {
		for (int i = 0; i < childs; ++ i) {
			const tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			const tpoint best_size = grid->get_best_size();
			if (best_size.x > result.x) {
				result.x = best_size.x;
			}

			row_height_[i] = best_size.y;
			if (best_size.y > result.y) {
				result.y = best_size.y;
			}
		}
		col_width_[0] = result.x;

	} else {
		// horizontal
		for (int i = 0; i < childs; ++ i) {
			const tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			if (grid->get_visible() == INVISIBLE) {
				continue;
			}

			const tpoint best_size = grid->get_best_size();

			col_width_[i] = best_size.x;
			result.x += best_size.x;

			if (best_size.y > result.y) {
				result.y = best_size.y;
			}
		}
		row_height_[0] = result.y;		
	}

	return result;
}

tpoint tstack::tgrid2::calculate_best_size_bh(const int width)
{
	/*
	 * The best size is the combination of the greatest width and greatest
	 * height.
	 */
	const int childs = children_vsize_;
	tpoint result(0, 0);
	if (stack_.mode_ == pip || stack_.mode_ == vertical) {
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			if (grid->get_visible() == INVISIBLE) {
				continue;
			}

			const tpoint best_size = grid->calculate_best_size_bh(width);
			if (best_size.x > result.x) {
				result.x = best_size.x;
			}

			row_height_[i] = best_size.y;
			if (stack_.mode_ != vertical) {
				if (best_size.y > result.y) {
					result.y = best_size.y;
				}
			} else {
				result.y += best_size.y;
			}
		}
		col_width_[0] = result.x;

	} else if (stack_.mode_ == radio) {
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			const tpoint best_size = grid->calculate_best_size_bh(width);
			if (best_size.x > result.x) {
				result.x = best_size.x;
			}

			row_height_[i] = best_size.y;
			if (best_size.y > result.y) {
				result.y = best_size.y;
			}
		}
		col_width_[0] = result.x;

	} else {
		// horizontal
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			if (grid->get_visible() == INVISIBLE) {
				continue;
			}

			const tpoint best_size = grid->calculate_best_size_bh(width);

			col_width_[i] = best_size.x;
			result.x += best_size.x;

			if (best_size.y > result.y) {
				result.y = best_size.y;
			}
		}
		row_height_[0] = result.y;

		if (result.x > width) {
			// by below calculate, width[0] is full width of left part. it maybe equal to screen_width, if there is multiline control.
			// but during running, left part will be clipped.
			result.x = width;
		}
	}

	return result;
}

void tstack::tgrid2::place(const tpoint& origin, const tpoint& size)
{
	int childs = children_vsize_;

	if (stack_.mode_ == pip) {
		twidget::place(origin, size);
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);

			if (grid->get_visible() == INVISIBLE) {
				// normal, it should continue.
				// but set_radio_layer don't trigger place, if continue here, will result place fail.
				// of course, don't continue will increase cpu load.
				continue;
			}
			grid->place(origin, size);
		}

	} else if (stack_.mode_ == radio) {
		twidget::place(origin, size);
		int visibled_at = nposm;
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);

			if (grid->get_visible() == VISIBLE) {
				if (visibled_at == nposm) {
					grid->place(origin, size);
					visibled_at = i;
				} else {
					// user maybe not call set_radio_layer
					tdisable_invalidate_layout_lock lock;
					grid->set_visible(INVISIBLE);
				}
			}
		}

	} else {
		tgrid::place(origin, size);
		// vertical/horizontal
		if (!stack_.upward_) {
			stack_.std_top_rect_ = null_rect;
			// once upward_ = false, layer(0) is set_orign/set_visible_area, reset to it.
			tupward_lock lock(stack_); // in order to fake move()
			stack_.calculate_reserve();
			const int top_rect_h = stack_.mode_ == vertical? stack_.std_top_rect_.h: stack_.std_top_rect_.w;
			stack_.move(-1 * (top_rect_h - stack_.reserve_height_));
		}
	}
}

void tstack::tgrid2::set_origin(const tpoint& origin)
{
	/*
	 * Set the origin for every item.
	 *
	 * @todo evaluate whether setting it only for the visible item is better
	 * and what the consequences are.
	 */

	if (stack_.mode_ == pip || stack_.mode_ == radio) {
		twidget::set_origin(origin);
		int childs = children_vsize_;
		for (int i = 0; i < childs; ++ i) {
			tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
			if (grid->get_visible() != twidget::VISIBLE) {
				continue;
			}
			grid->set_origin(origin);
		}

	} else {
		tgrid::set_origin(origin);
	}
}

twidget* tstack::tgrid2::find(const std::string& id, const bool must_be_active)
{
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		if (twidget* widget = grid->find(id, must_be_active)) {
			return widget;
		}
	}
	return NULL;
}

const twidget* tstack::tgrid2::find(const std::string& id, const bool must_be_active) const
{
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		if (const twidget* widget = grid->find(id, must_be_active)) {
			return widget;
		}
	}
	return NULL;
}

void tstack::tgrid2::set_visible_area(const SDL_Rect& area)
{
	twidget::set_visible_area(area);
	/*
	 * Set the visible area for every item.
	 *
	 * @todo evaluate whether setting it only for the visible item is better
	 * and what the consequences are.
	 */
	int childs = children_vsize_;

	for (int i = 0; i < childs; ++ i) {
		tgrid* grid = dynamic_cast<tgrid*>(children_[i].widget_);
		grid->set_visible_area(area);
	}
}

// tgrid3 indicate those grids that in stack.
void tstack::tgrid3::impl_draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	if (stack_.mode_ == pip) {
		if (auto_draw_children_) {
			tgrid::impl_draw_children(frame_buffer, x_offset, y_offset);
		} else {
			auto_draw_children_ = true;
		}
	} else {
		// twidget::impl_draw_background(frame_buffer, x_offset, y_offset);
	}
}

void tstack::tgrid3::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	if (stack_.mode_ == pip) {
		// do nothing. draw children task is execute during draw background.
		return;
	} else {
		tgrid::impl_draw_children(frame_buffer, x_offset, y_offset);
	}
}

void tstack::set_radio_layer(int layer)
{
	VALIDATE(mode_ == radio, null_str);

	tgrid::tchild* children = grid_.children();
	int childs = grid_.children_vsize();

	VALIDATE(layer < childs, "layer is invalid!");

	twindow* window = get_window();
	tdisable_invalidate_layout_lock lock;

	// if INVISIBLE, do nothing.
	bool changed = false;
	for (int n = 0; n < childs; ++ n) {
		tgrid& grid = *(dynamic_cast<tgrid*>(children[n].widget_));
		if (n != layer) {
			if (grid.get_visible() == VISIBLE) {
				changed = true;
				grid.set_visible(INVISIBLE);
			}
		} else if (grid.get_visible() != VISIBLE) {
			VALIDATE(grid.get_visible() == INVISIBLE, null_str);
			changed = true;
			grid.set_visible(twidget::VISIBLE);
			if (window->layouted()) {
				grid.place(get_origin(), get_size());
			}
			// if keyboard focus is in previous grid, change this grid force. 
			window->keyboard_capture(&grid);
		}
	}
}

tgrid* tstack::layer(int at) const 
{ 
	return dynamic_cast<tgrid*>(grid_.child(at).widget_); 
}

void tstack::finalize(const std::vector<std::pair<tbuilder_grid_const_ptr, unsigned> >& widget_builder)
{
	if (mode_ == vertical || mode_ == horizontal) {
		VALIDATE(widget_builder.size() == 2, null_str);
	}

	grid_.stack_init();

	for (std::vector<std::pair<tbuilder_grid_const_ptr, unsigned> >::const_iterator it = widget_builder.begin(); it != widget_builder.end(); ++ it) {
		const tbuilder_grid_const_ptr& builder = it->first;
		tgrid3* grid = new tgrid3(*this);
		builder->build(grid);

		grid_.stack_insert_child(*grid, nposm, it->second);
	}
}

const std::string& tstack::get_control_type() const
{
    static const std::string type = "stack";
    return type;
}

//
// vertical/horizontal
//
void tstack::set_splitter(twidget& splitter_bar, bool center_close)
{
	VALIDATE(!splitter_bar_ && (mode_ == vertical || mode_ == horizontal), null_str);
	top_grid_ = dynamic_cast<tstack::tgrid3*>(layer(0));
	bottom_grid_ = dynamic_cast<tstack::tgrid3*>(layer(1));

	splitter_bar_ = &splitter_bar;
	splitter_bar.connect_signal<event::LEFT_BUTTON_DOWN>(
		boost::bind(
			&tstack::signal_handler_left_button_down
			, this
			, _5));
	splitter_bar.connect_signal<event::MOUSE_LEAVE>(
		boost::bind(
			&tstack::signal_handler_mouse_leave
			, this
			, _5));
	splitter_bar.connect_signal<event::MOUSE_MOTION>(
		boost::bind(
			&tstack::signal_handler_mouse_motion
			, this
			, _5));
}

void tstack::signal_handler_left_button_down(const tpoint& coordinate)
{
	VALIDATE(is_null_coordinate(first_coordinate_), null_str);

	std_top_rect_ = null_rect;
	diff_ = tpoint(0, 0);

	bool can_drag = point_in_rect(coordinate.x, coordinate.y, splitter_bar_->get_rect());
	if (did_can_drag_) {
		can_drag = did_can_drag_(*this, coordinate.x, coordinate.y);
	}
	if (can_drag) {
		first_coordinate_ = coordinate;
		if (did_mouse_event_) {
			did_mouse_event_(*this, mouse_down, first_coordinate_, first_coordinate_);
		}
	}
}

void tstack::signal_handler_mouse_leave(const tpoint& last)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (!SDL_RectEmpty(&std_top_rect_)) {
		const int diff = mode_ == vertical? diff_.y: diff_.x;
		const int top_rect_h = mode_ == vertical? std_top_rect_.h: std_top_rect_.w;

		if (upward_) {
			if (diff < 0) {
				if (diff <= -1 * threshold_) {
					move(-1 * (top_rect_h - reserve_height_));
					upward_ = false;

				} else {
					move(0);
				}
			} else {
				// diff_.y isn't increase/decrease 1, exist segment when drag.
				// diff_.y of standard-line maybe in segment, and result in not triger by system.
				// so should reset standard-line again.
				move(0);
			}

		} else {
			if (diff > 0) {
				if (diff >= threshold_) {
					move(top_rect_h - reserve_height_);
					upward_ = true;

				} else {
					move(0);
				}
			} else {
				// diff_.y isn't increase/decrease 1, exist segment when drag.
				// diff_.y of standard-line maybe in segment, and result in not triger by system.
				// so should reset standard-line again.
				move(0);
			}
		}
	}
	
	const tpoint first = first_coordinate_;
	// did_mouse_event_ maybe call dragging().
	set_null_coordinate(first_coordinate_);

	if (did_mouse_event_) {
		did_mouse_event_(*this, mouse_leave, first, last);
	}
}

void tstack::calculate_reserve()
{
	VALIDATE(SDL_RectEmpty(&std_top_rect_), null_str);

	std_top_rect_ = top_grid_->get_rect();
	std_bottom_rect_ = bottom_grid_->get_rect();

	if (upward_) {
		const int normalize_size = mode_ == vertical? std_top_rect_.h: std_top_rect_.w;

		max_top_movable_ = 0;
		reserve_height_ = 0;
		threshold_ = normalize_size / 4;

		// now initial_upward_ must be true.

		// app maybe set a absolute value for reserve_y. for example 100.
		// max_top_movable_ = reserve_y - std_top_rect_.y, std_top_rect_.y will not same as when !initial_upward_.
		// it will result dismiss.
		// of corse, if reserve_y is calculate from control rect, they will same initial_upward_ and !initial_upward_.
		if (did_calculate_reserve_) {
			did_calculate_reserve_(*this, max_top_movable_, reserve_height_, threshold_);

			VALIDATE(reserve_height_ >= 0, null_str);
			VALIDATE(max_top_movable_ + reserve_height_ <= normalize_size, null_str);
			VALIDATE(threshold_ > 0 && threshold_ < normalize_size, null_str);
		}
	}
}

void tstack::signal_handler_mouse_motion(const tpoint& last)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	diff_ = tpoint(last.x - first_coordinate_.x, last.y - first_coordinate_.y);

	if (SDL_RectEmpty(&std_top_rect_)) {
		calculate_reserve();
	}

	move(mode_ == vertical? diff_.y: diff_.x);

	if (did_mouse_event_) {
		did_mouse_event_(*this, mouse_motion, first_coordinate_, last);
	}
}

void tstack::move(const int diff)
{
	// disable layout window when move.
	tdisable_invalidate_layout_lock lock;

	if (diff < 0) {
		if (!upward_) {
			return;
		}
		SDL_Rect rect0 = std_top_rect_;
		SDL_Rect rect1 = std_bottom_rect_;

		const int max_bottom_movable = (mode_ == vertical? std_top_rect_.h: std_top_rect_.w) - max_top_movable_ - reserve_height_;

		int abs_diff = abs(diff);
		//
		// move calendar
		//
		int move_top = 0;
		if (center_close_) {
			const int half = abs_diff / 2;

			move_top = half;
			const int move_bottom = abs_diff - move_top;
			if (move_bottom > max_bottom_movable) {
				// max_bottom_height < half, so require move top more pixels.
				move_top += move_bottom - max_bottom_movable;
			}
			if (move_top > max_top_movable_) {
				move_top = max_top_movable_;
			}

		} else {
			move_top = abs_diff;
			if (move_top > max_top_movable_) {
				move_top = max_top_movable_;
			}
		}

		if (mode_ == vertical) {
			rect0.h = std_top_rect_.h - abs_diff;
			if (rect0.h >= reserve_height_) {
				top_grid_->set_origin(tpoint(std_top_rect_.x, std_top_rect_.y - move_top));
				top_grid_->set_visible_area(rect0);
				top_grid_->set_dirty();

				//
				// move data
				//
				bottom_grid_->place(tpoint(std_bottom_rect_.x, std_bottom_rect_.y + diff), tpoint(std_bottom_rect_.w, std_bottom_rect_.h + abs_diff));
				bottom_grid_->set_dirty();
			}
		} else {
			rect0.w = std_top_rect_.w - abs_diff;
			if (rect0.w >= reserve_height_) {
				top_grid_->set_origin(tpoint(std_top_rect_.x - move_top, std_top_rect_.y));

				top_grid_->set_visible_area(rect0);
				top_grid_->set_dirty();

				//
				// move data
				//
				bottom_grid_->place(tpoint(std_bottom_rect_.x + diff, std_bottom_rect_.y), tpoint(std_bottom_rect_.w + abs_diff, std_bottom_rect_.h));
				bottom_grid_->set_dirty();
			}
		}

	} else if (diff > 0) {
		if (upward_) {
			return;
		}
		SDL_Rect rect0 = std_top_rect_;
		SDL_Rect rect1 = std_bottom_rect_;

		const int max_bottom_movable = (mode_ == vertical? std_top_rect_.h: std_top_rect_.w) - max_top_movable_ - reserve_height_;

		//
		// move calendar
		//
		// int move_top = 0, move_bottom = 0;
		int move_top = 0;
		if (center_close_) {
			const int half = diff / 2;

			move_top = half;
			const int move_bottom = diff - move_top;
			if (move_bottom > max_bottom_movable) {
				// max_bottom_height < half, so require move top more pixels.
				move_top += move_bottom - max_bottom_movable;
			}
			if (move_top > max_top_movable_) {
				move_top = max_top_movable_;
			}

		} else {
			if (diff < max_bottom_movable) {
				move_top = 0;
			} else {
				move_top = diff - max_bottom_movable;
			}
		}

		if (mode_ == vertical) {
			rect0.y += max_top_movable_;
			rect0.h = reserve_height_ + diff;
			if (rect0.h <= std_top_rect_.h) {
				top_grid_->set_origin(tpoint(std_top_rect_.x, std_top_rect_.y + move_top));
				top_grid_->set_visible_area(rect0);
				top_grid_->set_dirty();

				//
				// move data
				//
				bottom_grid_->place(tpoint(std_bottom_rect_.x, std_bottom_rect_.y + diff), tpoint(std_bottom_rect_.w, std_bottom_rect_.h - diff));
				bottom_grid_->set_dirty();
			}
		} else {
			rect0.x += max_top_movable_;
			rect0.w = reserve_height_ + diff;
			if (rect0.w <= std_top_rect_.w) {
				top_grid_->set_origin(tpoint(std_top_rect_.x + move_top, std_top_rect_.y));
				top_grid_->set_visible_area(rect0);
				top_grid_->set_dirty();

				//
				// move data
				//
				bottom_grid_->place(tpoint(std_bottom_rect_.x + diff, std_bottom_rect_.y), tpoint(std_bottom_rect_.w - diff, std_bottom_rect_.h));
				bottom_grid_->set_dirty();
			}
		}

	} else {
		// recover to original state.
		SDL_Rect clip_rect = std_top_rect_;
		bool redraw = false;
		if (!upward_) {
			if (mode_ == vertical) {
				clip_rect.y += max_top_movable_;
				if (clip_rect.h != reserve_height_) {
					// when upward == 0, original stat's clip_rect.h must be reserve_height_.
					clip_rect.h = reserve_height_;
					redraw = true;
				}
			} else {
				clip_rect.x += max_top_movable_;
				if (clip_rect.w != reserve_height_) {
					// when upward == 0, original stat's clip_rect.h must be reserve_height_.
					clip_rect.w = reserve_height_;
					redraw = true;
				}
			}
		}
		if (top_grid_->get_rect() != std_top_rect_) {
			top_grid_->set_origin(tpoint(std_top_rect_.x, std_top_rect_.y));
			top_grid_->set_dirty();
			redraw = true;

		}
		if (redraw) {
			// if only downward < max_bottom_movable, don't modify rect of top_grid_.
			top_grid_->set_visible_area(clip_rect);
		}

		if (bottom_grid_->get_rect() != std_bottom_rect_) {
			bottom_grid_->place(tpoint(std_bottom_rect_.x, std_bottom_rect_.y), tpoint(std_bottom_rect_.w, std_bottom_rect_.h));
			bottom_grid_->set_dirty();
		}
	}
}

} // namespace gui2

