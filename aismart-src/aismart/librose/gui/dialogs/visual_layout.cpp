/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "gui/dialogs/visual_layout.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/stack.hpp"

#include <boost/bind.hpp>
#include <numeric>

namespace gui2 {

REGISTER_DIALOG(rose, visual_layout)

tvisual_layout::tvisual_layout(const twindow& target, const std::string& reason)
	: target_(target)
	, reason_(reason)
	, stacked_color_(0xffff0000)
	, width_factor_(1)
	, height_factor_(1)
{
}

void tvisual_layout::pre_show()
{
	window_->set_label("misc/white-background.png");

	std::stringstream ss;
	const bool use_get_best_size = !target_.get_width() || !target_.get_height();

	tlabel* label = find_widget<tlabel>(window_, "title", false, true);
	ss.str("");
	if (use_get_best_size) {
		ss << "best size of ";
	} else {
		ss << "cell size of ";
	}
	ss << target_.id();
	label->set_label(ss.str());


	label = find_widget<tlabel>(window_, "reason", false, true);
	ss.str("");
	ss << reason_ << "\n";
	ss << _("If twinkle, it is stack, can click to switch layer.");
	label->set_label(ss.str());

	ttrack* layout = find_widget<ttrack>(window_, "layout", false, true);
	layout->set_label(null_str);
	layout->set_canvas_variable("ripple_image", variant("misc/ripple-green.png"));
	layout->set_did_draw(boost::bind(&tvisual_layout::did_layout_foreground, this, _1, _2, _3));
	layout->set_timer_interval(500);

	layout->connect_signal<event::MOUSE_MOTION>(
		boost::bind(
			&tvisual_layout::signal_handler_mouse_motion
			, this
			, boost::ref(*window_)
			, _5)
		, event::tdispatcher::front_child);

	layout->connect_signal<event::LEFT_BUTTON_UP>(
		boost::bind(
			&tvisual_layout::signal_handler_left_button_up
			, this
			, boost::ref(*window_)
			, _5)
		, event::tdispatcher::front_child);
}

bool tvisual_layout::is_extensible(const twidget& widget) const
{ 
	const std::string& type = widget.get_control_type();
	return type == "panel" || type == "scroll_panel" || type == "grid" || type == "stack";
}

void tvisual_layout::draw_grid_cell(SDL_Renderer* renderer, const tgrid& grid, const tpoint& origin2)
{
	const uint32_t render_size_color = 0xffff0000;
	SDL_Rect dst;

	const std::vector<unsigned>& row_height = grid.row_height_;
	const std::vector<unsigned>& col_width = grid.col_width_;

	tpoint origin = origin2;
	for (unsigned row = 0; row < grid.rows_; ++row) {
		if (row_height.size() <= row) {
			continue;
		}
		for (unsigned col = 0; col < grid.cols_; ++ col) {
			if (col_width.size() <= col) {
				continue;
			}

			const twidget* widget = grid.child(row, col).widget_;
			if (widget->get_visible() == twidget::INVISIBLE) {
				continue;
			}
			if (!is_extensible(*widget)) {
				dst = ::create_rect(origin.x, origin.y, col_width[col], row_height[row]);
				SDL_Rect dst2{(int)(dst.x * width_factor_), (int)(dst.y * height_factor_), (int)(dst.w * width_factor_), (int)(dst.h * height_factor_)};
				render_rect_frame(renderer, dst2, render_size_color);
				widgets_.insert(std::make_pair(widget, dst));

			} else {
				const tgrid* grid2 = nullptr;
				if (widget->get_control_type() == "grid") {
					grid2 = dynamic_cast<const tgrid*>(widget);

				} else if (widget->get_control_type() == "stack") {
					const tstack* stacked = dynamic_cast<const tstack*>(widget);
					std::map<const tstack*, int>::const_iterator it = stacks_.find(stacked);
					const int at = it != stacks_.end()? it->second: 0;
					grid2 = stacked->layer(at);

					// for get_best_size scenario, control of has best_size maybe one pixel and cann't hit. 
					dst = ::create_rect(origin.x, origin.y, col_width[col], row_height[row]);
					SDL_Rect dst2{(int)(dst.x * width_factor_), (int)(dst.y * height_factor_), (int)(dst.w * width_factor_), (int)(dst.h * height_factor_)};
					render_rect_frame(renderer, dst2, stacked_color_);
					widgets_.insert(std::make_pair(widget, dst));
					statk_in_widgets_.insert(std::make_pair(stacked, dst));

				} else {
					const tcontainer_* container = dynamic_cast<const tcontainer_*>(widget);
					grid2 = &container->layout_grid();
				}					
				draw_grid_cell(renderer, *grid2, origin);
			}
			origin.x += col_width[col];
		}
		origin.x = origin2.x;
		origin.y += row_height[row];
	}
}

void tvisual_layout::did_layout_foreground(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	bool use_get_best_size = false;
	int width = target_.get_width(), height = target_.get_height();
	if (!width || !height) {
		tpoint best_size = target_.get_best_size();
		width = best_size.x;
		height = best_size.y;
		use_get_best_size = true;
	}

	if (!width || !height) {
		return;
	}

	// twinkle stack widget.
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);
	if (!bg_drawn) {
		// SDL_RenderCopy(renderer, widget.background_texture().get(), nullptr, &widget_rect);
		stacked_color_ = stacked_color_ == 0xffff0000? 0xff00ff00: 0xffff0000;
		for (std::map<const tstack*, SDL_Rect>::const_iterator it = statk_in_widgets_.begin(); it != statk_in_widgets_.end(); ++ it) {
			SDL_Rect dst = it->second;
			dst.x = widget_rect.x + dst.x * width_factor_;
			dst.y = widget_rect.y + dst.y * height_factor_;
			dst.w *= width_factor_;
			dst.h *= height_factor_;
			render_rect_frame(renderer, dst, stacked_color_);
		}
		return;
	}
	stacked_color_ = 0xffff0000;

	widgets_.clear();
	statk_in_widgets_.clear();

	SDL_Rect dst = ::create_rect(0, 0, width, height);
	width_factor_ = 1.0 * widget_rect.w / width;
	height_factor_ = 1.0 * widget_rect.h / height;
	texture tex = SDL_CreateTexture(renderer, get_neutral_pixel_format().format, SDL_TEXTUREACCESS_TARGET, widget_rect.w, widget_rect.h);
	{
		trender_target_lock lock(renderer, tex);
		SDL_RenderClear(renderer);
		if (use_get_best_size) {
			render_rect_frame(renderer, dst, 0xff00ff00);
		}

		const tgrid& grid = target_.grid();
		draw_grid_cell(renderer, grid, tpoint(0, 0));
		SDL_SetTextureBlendMode(tex.get(), SDL_BLENDMODE_BLEND);
	}
	SDL_RenderCopy(renderer, tex.get(), nullptr, &widget_rect);
}

std::string tvisual_layout::generate_widget_str(const twidget& widget, const SDL_Rect& cell_rect)
{
	SDL_Rect target_client = target_.get_grid_rect();
	std::stringstream ss;

	ss << "id: ";
	if (!widget.id().empty()) {
		ss << widget.id();
	} else {
		ss << "--";
	}
	ss << ", type: " << widget.get_control_type();
	if (widget.get_control_type() == "stack") {
		const tstack* stacked = dynamic_cast<const tstack*>(&widget);
		std::map<const tstack*, int>::const_iterator it = stacks_.find(stacked);
		const int at = it != stacks_.end()? it->second: 0;

		ss << "(" << stacked->layers() << " layer->" << at << ")";
	}
	
	ss << ", cell rect: (" << cell_rect.x << "," << cell_rect.y << "," << cell_rect.w << "," << cell_rect.h << ")";

	ss << ", render rect: (" << (widget.get_x() - target_client.x) << "," << (widget.get_y() - target_client.y) << "," << widget.get_width() << "," << widget.get_height() << ")";

	// if it isn't stack, display first stack(click it will change)
	if (widget.get_control_type() != "stack") {
		twidget* tmp = widget.parent();
		while (tmp) {
			if (tmp->get_control_type() == "stack") {
				ss << " ***parent stacked: ";
				if (!tmp->id().empty()) {
					ss << tmp->id();
				} else {
					ss << "--";
				}
				break;
			}
			tmp = tmp->parent();
		}
	}

	return ss.str();
}

std::pair<const twidget*, SDL_Rect> tvisual_layout::which_hit(twindow& window, int x, int y) const
{
	ttrack& layout = find_widget<ttrack>(&window, "layout", false);
	int x2 = x - layout.get_x();
	int y2 = y - layout.get_y();

	SDL_Rect cell_rect;

	std::map<const twidget*, SDL_Rect> stackeds;
	std::map<const twidget*, SDL_Rect>::const_iterator hit = widgets_.end();
	for (hit = widgets_.begin(); hit != widgets_.end(); ++ hit) {
		const twidget* widget = hit->first;
		cell_rect = hit->second;
		cell_rect.x *= width_factor_;
		cell_rect.w *= width_factor_;
		cell_rect.y *= height_factor_;
		cell_rect.h *= height_factor_;
		if (point_in_rect(x2, y2, cell_rect)) {
			if (widget->get_control_type() == "stack") {
				stackeds.insert(std::make_pair(widget, hit->second));
				continue;
			}
			break;
		}
	}
	if (hit != widgets_.end()) {
		return std::make_pair(hit->first, hit->second);

	} else if (!stackeds.empty()) {
		while (stackeds.size() > 1) {
			for (hit = stackeds.begin(); hit != stackeds.end(); ++ hit) {
				const twidget* tmp = hit->first->parent();
				bool erased = false;
				while (tmp) {
					if (tmp->get_control_type() == "stack") {
						std::map<const twidget*, SDL_Rect>::iterator it = stackeds.find(tmp);
						if (it != stackeds.end()) {
							stackeds.erase(it);
							erased = true;
							break;
						}
					}
					tmp = tmp->parent();
				}
				if (erased) {
					break;
				}
			}
		}

		hit = stackeds.begin();
		return std::make_pair(hit->first, hit->second);
	}
	return std::make_pair(nullptr, null_rect);
}

void tvisual_layout::signal_handler_mouse_motion(twindow& window, const tpoint& coordinate)
{
	std::pair<const twidget*, SDL_Rect> hit = which_hit(window, coordinate.x, coordinate.y);
	
	tlabel& label = find_widget<tlabel>(&window, "tip", false);
	std::string tip = "no widget hit.";
	if (hit.first) {
		tip = generate_widget_str(*hit.first, hit.second);
	}
	label.set_label(tip);
}

void tvisual_layout::signal_handler_left_button_up(twindow& window, const tpoint& coordinate)
{
	std::pair<const twidget*, SDL_Rect> hit = which_hit(window, coordinate.x, coordinate.y);
	if (!hit.first) {
		return;
	}
	std::vector<const tstack*> stackeds;
	const twidget* tmp = hit.first;
	while (tmp) {
		if (tmp->get_control_type() == "stack") {
			stackeds.push_back(dynamic_cast<const tstack*>(tmp));
		}
		tmp = tmp->parent();
	}
	if (stackeds.empty()) {
		return;
	}
	for (std::vector<const tstack*>::const_iterator it = stackeds.begin(); it != stackeds.end(); ++ it) {
		if (stacks_.find(*it) == stacks_.end()) {
			stacks_.insert(std::make_pair(*it, 0));
		}
	}

	int at = 0;
	const tstack* stacked = stackeds[at];

	std::map<const tstack*, int>::iterator find = stacks_.find(stacked);
	VALIDATE(find != stacks_.end(), null_str);

	const int layers = find->first->layers();
	find->second = (find->second + 1) % layers;

	tlabel& label = find_widget<tlabel>(&window, "tip", false);
	label.set_label(generate_widget_str(*hit.first, hit.second));

	ttrack& layout = find_widget<ttrack>(&window, "layout", false);
	layout.set_dirty();
}

}
