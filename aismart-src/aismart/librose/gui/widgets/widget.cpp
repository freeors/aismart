/* $Id: widget.cpp 54218 2012-05-19 08:46:15Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/dialogs/dialog.hpp"
#include "rose_config.hpp"

namespace gui2 {

int cfg_2_os_size(const int cfg_size)
{
	if (game_config::mobile) {
		return cfg_size * gui2::twidget::hdpi_scale;
	} else {
		return cfg_size * gui2::twidget::hdpi_scale * 3 / 4;
	}
}

int os_size_2_cfg(const int os_size)
{
	if (game_config::mobile) {
		return os_size / gui2::twidget::hdpi_scale;
	} else {
		return os_size * 4 / (gui2::twidget::hdpi_scale * 3);
	}
}

int SCROLLBAR_BEST_SIZE = 0;
int SCROLLBAR2_BEST_SIZE = 0;

int twidget::hdpi_scale = 1;
const int twidget::max_effectable_point = 540; // 5.5-inch: 2208x1242, 5.8-inch: 2436x1125.
// rule: if should_conside_orientation() is true, gui2 should ignore current_landscape.
bool twidget::current_landscape = true;

const twidget* twidget::fire_event = nullptr;
twidget* twidget::link_group_owner = nullptr;
bool twidget::clear_restrict_width_cell_size = false;
bool twidget::disalbe_invalidate_layout = false;

bool twidget::landscape_from_orientation(torientation orientation, bool def)
{
	if (orientation != auto_orientation) {
		return orientation == landscape_orientation;
	}
	return def;
}

bool twidget::should_conside_orientation(const int width, const int height)
{
	const int max_effectable_pixel = hdpi_scale * max_effectable_point;
	return width <= max_effectable_pixel || height <= max_effectable_pixel;
}

tpoint twidget::orientation_swap_size(int width, int height)
{
	if (!should_conside_orientation(width, height)) {
		// force use landscape
		if (width < height) {
			int tmp = width;
			width = height;
			height = tmp;
		}

		return tpoint(width, height);
	}
	if (!current_landscape) {
		int tmp = width;
		width = height;
		height = tmp;
	}
	return tpoint(width, height);
}

twidget::twidget()
	: id_("")
	, parent_(NULL)
	, x_(0)
	, y_(0)
	, w_(0)
	, h_(0)
	, dirty_(true)
	, redraw_(false)
	, visible_(VISIBLE)
	, drawing_action_(DRAWN)
	, clip_rect_(empty_rect)
	, cookie_(nposm)
	, layout_size_(tpoint(0, 0))
	, linked_group_()
	, float_widget_(false)
	, restrict_width_(false)
	, terminal_(true)
	, allow_handle_event_(true)
	, can_invalidate_layout_(false)
{
}

twidget::~twidget()
{
	twidget* p = parent();
	while (p) {
		fire2(event::NOTIFY_REMOVAL, *p);
		p = p->parent();
	}

	if (this == fire_event) {
		fire_event = nullptr;
	}

	twindow* window = get_window();

	if (window) {
		if (!linked_group_.empty()) {
			window->remove_linked_widget(linked_group_, this);
		}
		tdialog* dialog = window->dialog();
		if (dialog) {
			dialog->destruct_widget(this);
		}
	}

}

void twidget::set_id(const std::string& id)
{
	id_ = id;
}

void twidget::layout_init(bool linked_group_only)
{
	if (!linked_group_only) {
		layout_size_ = tpoint(0,0);
		if (!linked_group_.empty()) {
			link_group_owner->add_linked_widget(linked_group_, *this, linked_group_only);
		}
	} else if (!linked_group_.empty()) {
		twindow* window = get_window();
		VALIDATE(link_group_owner != window, null_str);
		link_group_owner->add_linked_widget(linked_group_, *this, linked_group_only);
	}
}

tpoint twidget::get_best_size() const
{
	tpoint result = layout_size_;
	if (result == tpoint(0, 0)) {
		result = calculate_best_size();
	}

	return result;	
}

void twidget::place(const tpoint& origin, const tpoint& size)
{
	// x, y maybe < 0. for example: content_grid_.
	VALIDATE(size.x >= 0 && size.y >= 0, null_str);

	x_ = origin.x;
	y_ = origin.y;
	w_ = size.x;
	h_ = size.y;

	set_dirty();
}

void twidget::set_width(const int width)
{
	// release of visutal studio cannot detect assert, use breakpoint.
	VALIDATE(width >= 0, null_str);

	if (width == w_) {
		return;
	}

	w_ = width;
	set_dirty();
}

void twidget::set_size(const tpoint& size)
{
	// release of visutal studio cannot detect assert, use breakpoint.
	VALIDATE(size.x >= 0 && size.y >= 0, null_str);

	w_ = size.x;
	h_ = size.y;

	set_dirty();
}

twidget* twidget::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (!allow_handle_event_) {
		return nullptr;
	}
	return is_at(coordinate, must_be_active)? this : nullptr;
}

SDL_Rect twidget::get_dirty_rect() const
{
	return drawing_action_ == DRAWN? get_rect(): clip_rect_;
}

void twidget::move(const int x_offset, const int y_offset)
{
	x_ += x_offset;
	y_ += y_offset;
}

twindow* twidget::get_window()
{
	// Go up into the parent tree until we find the top level
	// parent, we can also be the toplevel so start with
	// ourselves instead of our parent.
	twidget* result = this;
	while (result->parent_) {
		result = result->parent_;
	}

	// on error dynamic_cast return 0 which is what we want.
	return dynamic_cast<twindow*>(result);
}

const twindow* twidget::get_window() const
{
	// Go up into the parent tree until we find the top level
	// parent, we can also be the toplevel so start with
	// ourselves instead of our parent.
	const twidget* result = this;
	while (result->parent_) {
		result = result->parent_;
	}

	// on error dynamic_cast return 0 which is what we want.
	return dynamic_cast<const twindow*>(result);
}

tdialog* twidget::dialog()
{
	twindow* window = get_window();
	return window ? window->dialog() : NULL;
}

void twidget::populate_dirty_list(twindow& caller, std::vector<twidget*>& call_stack)
{
	VALIDATE(call_stack.empty() || call_stack.back() != this, null_str);

	if (visible_ == INVISIBLE || (!dirty_ && !redraw_ && visible_ == HIDDEN)) {
		// when change from VISIBLE to HIDDEN, require populate it to dirty list.
		return;
	}

	if (get_drawing_action() == NOT_DRAWN) {
		return;
	}

	call_stack.push_back(this);

	if (dirty_ || exist_anim() || redraw_) {
		caller.add_to_dirty_list(call_stack);
	} else {
		// virtual function which only does something for container items.
		child_populate_dirty_list(caller, call_stack);
	}
}

void twidget::set_layout_size(const tpoint& size) 
{
	layout_size_ = size; 
}

void twidget::trigger_invalidate_layout(twindow* window)
{
	if (!window || !window->layouted()) {
		// when called by tbuilder_control, window will be nullptr.
		return;
	}

	if (!disalbe_invalidate_layout && !float_widget_) {
		if (window && window->layouted()) {
			// this maybe equal window
			twidget* tmp = this;
			while (tmp) {
				if (tmp->can_invalidate_layout()) {
					tmp->invalidate_layout(this);
					break;
				}
				tmp = tmp->parent();
			}
		}
	}
}

void twidget::set_visible(const tvisible visible)
{
	if (visible == visible_) {
		return;
	}

	if (!float_widget_) {
		// float widget allow to set visible always.
		// float widget's parent maybe not window, for example scrollbar.
		twidget* tmp = this;
		while (tmp) {
			tmp->validate_visible_pre_change(*this);;
			tmp = tmp->parent();
		}
	}
	

	// Switching to or from invisible should invalidate the layout.
	const bool need_resize = visible_ == INVISIBLE || visible == INVISIBLE;
	visible_ = visible;

	if (need_resize) {
		trigger_invalidate_layout(get_window());
	}

	redraw_ = true;
	dirty_ = true;
}

twidget::tdrawing_action twidget::get_drawing_action() const
{
	return (w_ == 0 || h_ == 0)? NOT_DRAWN: drawing_action_;
}

void twidget::set_origin(const tpoint& origin)
{
	if (origin.x == x_ && origin.y == y_) {
		return;
	}

	x_ = origin.x;
	y_ = origin.y;

	redraw_ = true;
}

void twidget::set_visible_area(const SDL_Rect& area)
{
	SDL_Rect original_clip_rect_ = clip_rect_;
	tdrawing_action original_action = drawing_action_;

	clip_rect_ = intersect_rects(area, get_rect());

	if (clip_rect_ == get_rect()) {
		drawing_action_ = DRAWN;
	} else if (clip_rect_ == empty_rect) {
		drawing_action_ = NOT_DRAWN;
	} else {
		drawing_action_ = PARTLY_DRAWN;
	}

	if (!redraw_ && drawing_action_ != NOT_DRAWN) {
		if (drawing_action_ != original_action) {
			redraw_ = true;

		} else if (drawing_action_ == PARTLY_DRAWN && original_clip_rect_ != clip_rect_) {
			redraw_ = true;
		}
	}
}

void twidget::set_dirty()
{
	dirty_ = true;
}

void twidget::clear_dirty()
{
	dirty_ = false;
	redraw_ = false;
}

void twidget::dirty_under_rect(const SDL_Rect& clip)
{
	VALIDATE(terminal_, null_str);

	SDL_Rect rect = get_dirty_rect();
	if (SDL_HasIntersection(&clip, &rect)) {
		redraw_ = true;
	}
}

SDL_Rect twidget::calculate_blitting_rectangle(
		  const int x_offset
		, const int y_offset)
{
	SDL_Rect result = get_rect();
	result.x += x_offset;
	result.y += y_offset;
	return result;
}

SDL_Rect twidget::calculate_clipping_rectangle(const int x_offset, const int y_offset)
{
	SDL_Rect clip_rect = clip_rect_;
	clip_rect.x += x_offset;
	clip_rect.y += y_offset;

	SDL_Rect surf_clip_rect;
	SDL_RenderGetClipRect(get_renderer(), &surf_clip_rect);
	if (SDL_RectEmpty(&surf_clip_rect)) {
		surf_clip_rect.w = w_;
		surf_clip_rect.h = h_;
	}
	if (surf_clip_rect.w != w_ || surf_clip_rect.h != h_) {
		return intersect_rects(clip_rect, surf_clip_rect);
	}
	return clip_rect;
}

void twidget::draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	VALIDATE(w_ > 0 && h_ > 0, null_str);

	// for example, tree_view_node only draw top grid's part.
	// SDL_Rect task_rect = get_draw_background_rect();
	SDL_Rect task_rect = get_rect();

	if (drawing_action_ == PARTLY_DRAWN) {
		SDL_Rect clipping_rect = calculate_clipping_rectangle(x_offset, y_offset);
		if (SDL_RectEmpty(&clipping_rect)) {
			return;
		}
		if (task_rect.x != x_ || task_rect.y != y_ || task_rect.w != w_ || task_rect.h != h_) {
			clipping_rect = intersect_rects(clipping_rect, task_rect);
			if (SDL_RectEmpty(&clipping_rect)) {
				return;
			}
		}
		
		texture_clip_rect_setter clip(&clipping_rect);
		impl_draw_background(frame_buffer, x_offset, y_offset);

	} else if (task_rect.x != x_ || task_rect.y != y_ || task_rect.w != w_ || task_rect.h != h_) {
		SDL_Rect clipping_rect;
		SDL_RenderGetClipRect(get_renderer(), &clipping_rect);
		if (!SDL_RectEmpty(&clipping_rect)) {
			clipping_rect = intersect_rects(clipping_rect, task_rect);
			if (SDL_RectEmpty(&clipping_rect)) {
				return;
			}
		} else {
			clipping_rect = task_rect;
		}

		texture_clip_rect_setter clip(&clipping_rect);
		impl_draw_background(frame_buffer, x_offset, y_offset);

	} else {
		impl_draw_background(frame_buffer, x_offset, y_offset);
	}
}

void twidget::draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	if (drawing_action_ == PARTLY_DRAWN) {
		const SDL_Rect clipping_rect = calculate_clipping_rectangle(x_offset, y_offset);
		if (SDL_RectEmpty(&clipping_rect)) {
			return;
		}

		texture_clip_rect_setter clip(&clipping_rect);
		impl_draw_children(frame_buffer, x_offset, y_offset);
	} else {
		impl_draw_children(frame_buffer, x_offset, y_offset);
	}
}

bool twidget::is_at(const tpoint& coordinate, const bool must_be_active) const
{
	if (visible_ != VISIBLE) {
		return false;
	}

	if (drawing_action_ == NOT_DRAWN) {
		return false;
	} else if (drawing_action_ == PARTLY_DRAWN) {
		return point_in_rect(coordinate.x, coordinate.y, clip_rect_);
	}

	return coordinate.x >= x_
			&& coordinate.x < (x_ + w_)
			&& coordinate.y >= y_
			&& coordinate.y < (y_ + h_) ? true : false;
}

//
// function session.
//
void blit_integer_blits(std::vector<tformula_blit>& blits, const int canvas_width, const int canvas_height, const int x, const int y, int integer)
{
	const int digit_width = 8;
	const int digit_height = 12;
	if (canvas_width < 8) {
		return;
	}
	if (canvas_height < digit_height) {
		return;
	}
	if (integer < 0) {
		integer *= -1;
	}

	std::stringstream ss;
	SDL_Rect dst_clip = ::create_rect(x, y, 0, 0);

	int digit, max = 10;
	while (max <= integer) {
		max *= 10;
	}

	do {
		max /= 10;
		if (max) {
			digit = integer / max;
			integer %= max;
		} else {
			digit = integer;
			integer = 0;
		}

		ss.str("");
		ss << "misc/digit.png~CROP(" << (8 * digit) << ", 0, 8, 12)";
		blits.push_back(tformula_blit(ss.str(), dst_clip.x, dst_clip.y, 0, 0));

		dst_clip.x += digit_width;
		if (dst_clip.x > canvas_width) {
			break;
		}
	} while (max > 1);
}

void generate_integer2_blits(std::vector<tformula_blit>& blits, int width, int height, const std::string& img, int integer, bool greyscale)
{
	surface surf = image::get_image(img);
	if (!surf) {
		return;
	}

	bool use_surface = greyscale;
	if (use_surface) {
		if (greyscale) {
			surf = greyscale_image(surf);
		}
		blits.push_back(tformula_blit(surf, 0, 0, width, height));
	} else {
		blits.push_back(tformula_blit(img, 0, 0, width, height));
	}

	if (integer > 0) {
		blit_integer_blits(blits, width, height, 0, 0, integer);
	}
}

void generate_pip_blits(std::vector<tformula_blit>& blits, const std::string& bg, const std::string& fg)
{
	surface fg_surf = image::get_image(fg);
	if (fg_surf) {
		blits.push_back(tformula_blit(bg, null_str, null_str, "(width)", "(height)"));
		std::string fg_width = str_cast(fg_surf->w), fg_height = str_cast(fg_surf->h);

		// (if(width >= fg_width, (width - fg_width)/2, 0);
		std::stringstream x;
		x << "(if(width >= " << fg_width << ", (width - " << fg_width << ")/2, 0)";

		// "(if(height >= fg_height, (height - fg_height)/2, 0)";
		std::stringstream y;
		y << "(if(height >= " << fg_height << ", (height - " << fg_height << ")/2, 0)";
		blits.push_back(tformula_blit(fg, x.str(), y.str(), fg_width, fg_height));
	}
}

void generate_pip_blits2(std::vector<tformula_blit>& blits, int width, int height, const std::string& bg, const std::string& fg)
{
	surface bg_surf = image::get_image(bg);
	surface fg_surf = image::get_image(fg);
	surface result = generate_pip_surface(bg_surf, fg_surf);

	blits.push_back(tformula_blit(result, 0, 0, width, height));
}

} // namespace gui2
