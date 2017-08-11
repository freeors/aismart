/* $Id: container.cpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#include "gui/widgets/container.hpp"

#include "formula_string_utils.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

SDL_Rect tcontainer_::get_grid_rect() const 
{ 
	const tspace4 margin_size = margin();

	SDL_Rect result = get_rect();

	result.x += margin_size.left;
	result.y += margin_size.top;
	result.w -= margin_size.left + margin_size.right;
	result.h -= margin_size.top + margin_size.bottom;

	return result; 
}

void tcontainer_::layout_init(bool linked_group_only)
{
	// Inherited.
	tcontrol::layout_init(linked_group_only);

	grid_->layout_init(linked_group_only);
}

tpoint tcontainer_::calculate_best_size_bh(const int width)
{
	const tspace4 margin_size = margin();
	tpoint result(grid_->calculate_best_size_bh(width - (margin_size.left + margin_size.right)));

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	if (result.x) {
		result.x += margin_size.left + margin_size.right;
	}

	if (result.y) {
		result.y += margin_size.top + margin_size.bottom;
	}

	return result;
}

void tcontainer_::place(const tpoint& origin, const tpoint& size)
{
	tcontrol::place(origin, size);

	const SDL_Rect rect = get_grid_rect();
	const tpoint client_size(rect.w, rect.h);
	const tpoint client_position(rect.x, rect.y);
	grid_->place(client_position, client_size);
}

tpoint tcontainer_::calculate_best_size() const
{
	tpoint result(grid_->get_best_size());
	const tspace4 border_size = margin();

	// If the best size has a value of 0 it's means no limit so don't
	// add the border_size might set a very small best size.
	// if (result.x) {
		result.x += border_size.left + border_size.right;
	// }

	// if (result.y) {
		result.y += border_size.top + border_size.bottom;
	// }

	return result;
}

void tcontainer_::set_origin(const tpoint& origin)
{
	// Inherited.
	twidget::set_origin(origin);

	const SDL_Rect rect = get_grid_rect();
	const tpoint client_position(rect.x, rect.y);
	grid_->set_origin(client_position);
}

void tcontainer_::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	grid_->set_visible_area(area);
}

void tcontainer_::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE && grid_->get_visible() == twidget::VISIBLE, null_str);

	grid_->draw_children(frame_buffer, x_offset, y_offset);
}

void tcontainer_::broadcast_frame_buffer(texture& frame_buffer)
{
	grid_->broadcast_frame_buffer(frame_buffer);
}

void tcontainer_::clear_texture()
{
	tcontrol::clear_texture();
	grid_->clear_texture();
}

void tcontainer_::layout_children()
{
	grid_->layout_children();
}

void tcontainer_::child_populate_dirty_list(twindow& caller,
		const std::vector<twidget*>& call_stack)
{
	std::vector<twidget*> child_call_stack = call_stack;
	grid_->populate_dirty_list(caller, child_call_stack);
}

bool tcontainer_::popup_new_window()
{
	return grid_->popup_new_window();
}

void tcontainer_::set_margin(int left, int right, int top, int bottom)
{
	if (left != nposm) {
		explicit_margin_.left = left;
	}
	if (right != nposm) {
		explicit_margin_.right = right;
	}
	if (top != nposm) {
		explicit_margin_.top = top;
	}
	if (bottom != nposm) {
		explicit_margin_.bottom = bottom;
	}
}

tspace4 tcontainer_::margin() const
{
	tspace4 conf_margin = mini_conf_margin();

	return tspace4{explicit_margin_.left != nposm? explicit_margin_.left: conf_margin.left, 
		explicit_margin_.right != nposm? explicit_margin_.right: conf_margin.right, 
		explicit_margin_.top != nposm? explicit_margin_.top: conf_margin.top, 
		explicit_margin_.bottom != nposm? explicit_margin_.bottom: conf_margin.bottom};
}

void tcontainer_::set_active(const bool active)
{
	// Not all our children might have the proper state so let them run
	// unconditionally.
	grid_->set_active(active);

	if(active == get_active()) {
		return;
	}

	set_dirty();

	set_self_active(active);
}

bool tcontainer_::disable_click_dismiss() const
{
	return tcontrol::disable_click_dismiss() && grid_->disable_click_dismiss();
}

void tcontainer_::init_grid(const boost::intrusive_ptr<tbuilder_grid>& grid_builder)
{
	VALIDATE(initial_grid().get_rows() == 0 && initial_grid().get_cols() == 0, null_str);

	grid_builder->build(&initial_grid());

	initial_subclass();
}

} // namespace gui2

