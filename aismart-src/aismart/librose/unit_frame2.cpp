/* $Id: unit_frame.cpp 47820 2010-12-05 18:08:51Z mordante $ */
/*
   Copyright (C) 2006 - 2010 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file */

#include "global.hpp"

#include "display.hpp"
#include "halo.hpp"
#include "sound.hpp"
#include "unit_frame.hpp"
#include "filter_tag.hpp"

#include "rose_config.hpp"

bool verify_render_image(int x, int y, const image::tblit& image, const SDL_Rect& clip_rect)
{
	VALIDATE(image.surf && image.width && image.height, null_str);

	SDL_Rect image_rect = create_rect(x, y, image.width, image.height);
	return rects_overlap(image_rect, clip_rect);
}

void unit_frame::redraw(const int frame_time,bool first_time,const map_location & src,const map_location & dst,int*halo_id,const frame_parameters & animation_val,const frame_parameters & engine_val)const
{
	const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	if (current_data.area_mode) {
		redraw_area_mode(frame_time, first_time, src, current_data);
		return;
	}

	const int xsrc = display::get_singleton()->loc_2_screen_x(src);
	const int ysrc = display::get_singleton()->loc_2_screen_y(src);
	const int xdst = display::get_singleton()->loc_2_screen_x(dst);
	const int ydst = display::get_singleton()->loc_2_screen_y(dst);

	const map_location::DIRECTION direction = src.get_relative_dir(dst);

	double tmp_offset_x = current_data.offset_x;
	double tmp_offset_y = current_data.offset_y;

		// debug code allowing to see the number of frames and their position
		// you need to add a '/n'
		// if (tmp_offset) std::cout << (int)(tmp_offset*100) << ",";

	display& disp = *display::get_singleton();
	int d2 = disp.zoom() / 2;
	if (first_time ) {
		// stuff sthat should be done only once per frame
		if (!current_data.sound.empty()  ) {
			sound::play_sound(sound_filter_tag::filter(current_data.sound, current_data.sound_filter));
		}
		if (!current_data.text.empty()  ) {
			display::get_singleton()->float_label(src,current_data.text,
			(current_data.text_color & 0x00FF0000) >> 16,
			(current_data.text_color & 0x0000FF00) >> 8,
			(current_data.text_color & 0x000000FF) >> 0);
		}
	}
	image::locator image_loc;
	if (!((src.x - dst.x) % 2) && src.y == dst.y) {
		image_loc = image::locator(current_data.image_horizontal, current_data.image_mod);
	}
	if ((image_loc.is_void() || image_loc.get_filename().empty()) && direction != map_location::NORTH && direction != map_location::SOUTH) {
		image_loc = image::locator(current_data.image_diagonal, current_data.image_mod);
	}
	if (image_loc.is_void() || image_loc.get_filename().empty()) { // invalid diag image, or not diagonal
		image_loc = image::locator(current_data.image, current_data.image_mod);
	}

	surface image;
	image::tblit material;
	if (!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
		image = image::get_image(image_loc);
		if (image.get() != NULL) {
			material = image::tblit(image, 0, 0, image::calculate_scaled_to_zoom(image->w), material.height = image::calculate_scaled_to_zoom(image->h));

			material.loc = &image::get_locator(image_loc);
			material.loc_type = image::SCALED_TO_ZOOM;
		}
	}

	const int x = static_cast<int>(tmp_offset_x * xdst + (1.0-tmp_offset_x) * xsrc) + d2;
	const int y = static_cast<int>(tmp_offset_y * ydst + (1.0-tmp_offset_y) * ysrc) + d2;
	const double zoom_factor = disp.get_zoom_factor();

	if (image != NULL) {
		VALIDATE(material.width && material.height, null_str);

		bool facing_west = direction == map_location::NORTH_WEST || direction == map_location::SOUTH_WEST;
		bool facing_north = direction == map_location::NORTH_WEST || direction == map_location::NORTH || direction == map_location::NORTH_EAST;
		if(!current_data.auto_hflip) facing_west = false;
		if(!current_data.auto_vflip) facing_north = true;
		int my_x = x + current_data.x * zoom_factor - material.width / 2;
		int my_y = y + current_data.y * zoom_factor - material.height / 2;
		if (facing_west) {
			my_x += current_data.directional_x;
		} else {
			my_x -= current_data.directional_x;
		}
		if (facing_north) {
			my_y += current_data.directional_y;
		} else {
			my_y -= current_data.directional_y;
		}

		display& disp = *display::get_singleton();
		if (verify_render_image(my_x, my_y, material, disp.main_map_view_rect())) {
			disp.render_image( my_x,my_y,
			       	static_cast<display::tdrawing_layer>(display::LAYER_UNIT_FIRST+current_data.drawing_layer),
			       	src, material, facing_west, false,
				ftofxp(current_data.highlight_ratio), current_data.blend_with,
			       	current_data.blend_ratio,current_data.submerge,!facing_north);
		}
		return;
	}

	// must be either image or halo.
	VALIDATE(!image, null_str);

	halo::remove(*halo_id);
	*halo_id = halo::NO_HALO;

	if(!current_data.halo.empty()) {
		halo::ORIENTATION orientation;
		switch(direction)
		{
			case map_location::NORTH:
			case map_location::NORTH_EAST:
				orientation = halo::NORMAL;
				break;
			case map_location::SOUTH_EAST:
			case map_location::SOUTH:
				if(!current_data.auto_vflip) {
					orientation = halo::NORMAL;
				} else {
					orientation = halo::VREVERSE;
				}
				break;
			case map_location::SOUTH_WEST:
				if (!current_data.auto_hflip && !current_data.auto_vflip ) {
					orientation = halo::NORMAL;

				} else if (!current_data.auto_hflip) {
					orientation = halo::VREVERSE;

				} else if (!current_data.auto_vflip) {
					orientation = halo::HREVERSE;

				} else {
					orientation = halo::HVREVERSE;
				}
				break;
			case map_location::NORTH_WEST:
				if (!current_data.auto_hflip) {
					orientation = halo::NORMAL;
				} else {
					orientation = halo::HREVERSE;
				}
				break;
			case map_location::NDIRECTIONS:
			default:
				orientation = halo::NORMAL;
				break;
		}

		// x/y_is_center = true, reference to halo::effect
		// halo::effect process zoom image.
		if (direction != map_location::SOUTH_WEST && direction != map_location::NORTH_WEST) {
			*halo_id = halo::add(static_cast<int>(x + current_data.x * zoom_factor),
					static_cast<int>(y + current_data.y * zoom_factor), true,
					current_data.halo + current_data.image_mod,
					map_location(-1, -1),
					orientation);
		} else {
			*halo_id = halo::add(static_cast<int>(x - current_data.x * zoom_factor),
					static_cast<int>(y + current_data.y * zoom_factor), true,
					current_data.halo + current_data.image_mod,
					map_location(-1, -1),
					orientation);
		}
	}
}

void unit_frame::redraw_area_mode(const int frame_time, bool first_time, const map_location& src, const frame_parameters& current_data) const
{
	// const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	double tmp_offset_x = current_data.offset_x;
	double tmp_offset_y = current_data.offset_y;

	display& disp = *display::get_singleton();
	const bool map = anim2::rt.type == anim_map;
	const int hdpi_scale = map? 1: gui2::twidget::hdpi_scale;
	image::locator image_loc = image::locator(current_data.image, current_data.image_mod);

	if (first_time ) {
		// stuff sthat should be done only once per frame
		if (!current_data.sound.empty()) {
			sound::play_sound(current_data.sound);
		}
		if (!current_data.text.empty()) {
			VALIDATE(map, null_str);
			disp.float_label(src, current_data.text,
				(current_data.text_color & 0x00FF0000) >> 16,
				(current_data.text_color & 0x0000FF00) >> 8,
				(current_data.text_color & 0x000000FF) >> 0, 
				true);
		}
	}

	surface image;
	int image_width = 0, image_height = 0;
	image::tblit material;
	if (!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
		if (map) {
			image = image::get_image(image_loc);
			if (image.get() != NULL) {
				material = image::tblit(image, 0, 0, image::calculate_scaled_to_zoom(image.get()->w), image::calculate_scaled_to_zoom(image.get()->h));

				material.loc = &image::get_locator(image_loc);
				material.loc_type = image::SCALED_TO_ZOOM;
			}
		} else {
			image = image::get_image(image_loc);
			if (image.get() != NULL) {
				material = image::tblit(image, 0, 0, image.get()->w * anim2::rt.zoomx, image.get()->h * anim2::rt.zoomy);

				material.loc = &image::get_locator(image_loc);
				material.loc_type = image::UNSCALED;
			}
		}
	}

	SDL_Rect map_area = map? disp.main_map_widget_rect(): anim2::rt.rect;

	double zoom_factor_x = map? disp.get_zoom_factor(): anim2::rt.zoomx;
	double zoom_factor_y = map? zoom_factor_x: anim2::rt.zoomy;

	const int x = static_cast<int>(tmp_offset_x * map_area.w) + (map? 0: map_area.x);
	const int y = static_cast<int>(tmp_offset_y * map_area.h) + (map? 0: map_area.y);

	bool facing_west = false;
	bool facing_north = true;

	const map_location zero_loc(0,0);
	if (map) {
		zero_x_ = disp.loc_2_screen_x(zero_loc);
		zero_y_ = disp.loc_2_screen_y(zero_loc);
	}

	tdrawing_buffer* drawing = nullptr;
	if (map) {
		drawing = &disp;
	} else {
		drawing = anim2::manager::instance;
	}
	if (image != NULL) {
		VALIDATE(material.width && material.height, null_str);

		const int my_x = x + int(current_data.x * hdpi_scale * zoom_factor_x - material.width/2);
		const int my_y = y + int(current_data.y * hdpi_scale * zoom_factor_y - material.height/2);

		if (verify_render_image(my_x, my_y, material, map_area)) {
			drawing->render_image(my_x, my_y,
			       	static_cast<display::tdrawing_layer>(display::LAYER_UNIT_FIRST+current_data.drawing_layer),
			       	map_location(), material, facing_west, false,
					ftofxp(current_data.highlight_ratio), current_data.blend_with,
			       	current_data.blend_ratio, current_data.submerge, !facing_north);
		}

	} else if (!current_data.stext.empty()) {
		int my_x = x + int(current_data.x * hdpi_scale * zoom_factor_x);
		int my_y = y + int(current_data.y * hdpi_scale * zoom_factor_y);

		drawing->draw_text_in_hex2(map_location(), static_cast<display::tdrawing_layer>(display::LAYER_UNIT_FIRST+current_data.drawing_layer),
			current_data.stext, current_data.font_size, uint32_to_color(current_data.text_color), my_x, my_y,
			       	ftofxp(current_data.highlight_ratio), true);
	}
}

std::set<map_location> unit_frame::get_overlaped_hex(const int frame_time,const map_location & src,const map_location & dst,const frame_parameters & animation_val,const frame_parameters & engine_val) const
{
	const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	if (current_data.area_mode) {
		return get_overlaped_hex_area_mode(frame_time, current_data);
	}

	display* disp = display::get_singleton();
	const int xsrc = disp->loc_2_screen_x(src);
	const int ysrc = disp->loc_2_screen_y(src);
	const int xdst = disp->loc_2_screen_x(dst);
	const int ydst = disp->loc_2_screen_y(dst);
	const map_location::DIRECTION direction = src.get_relative_dir(dst);

	// const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	double tmp_offset_x = current_data.offset_x;
	double tmp_offset_y = current_data.offset_y;
	int d2 = display::get_singleton()->zoom() / 2;

	image::locator image_loc;
	if (!((src.x - dst.x) % 2) && src.y == dst.y) {
		image_loc = current_data.image_horizontal;
	}
	if ((image_loc.is_void() || image_loc.get_filename().empty()) && direction != map_location::NORTH && direction != map_location::SOUTH) {
		image_loc = current_data.image_diagonal;
	}
	if (image_loc.is_void() || image_loc.get_filename().empty()) { // invalid diag image, or not diagonal
		image_loc = current_data.image;
	}

	// we always invalidate our own hex because we need to be called at redraw time even
	// if we don't draw anything in the hex itself
	std::set<map_location> result;
	if(tmp_offset_x == 0 && tmp_offset_y == 0 && current_data.x == 0 && current_data.directional_x == 0 && image::is_in_hex(image_loc)) {
		result.insert(src);
		int my_y = current_data.y;
		bool facing_north = direction == map_location::NORTH_WEST || direction == map_location::NORTH || direction == map_location::NORTH_EAST;
		if(!current_data.auto_vflip) facing_north = true;
		if(facing_north) {
			my_y += current_data.directional_y;
		} else {
			my_y -= current_data.directional_y;
		}
		if(my_y < 0) {
			result.insert(src.get_direction(map_location::NORTH));
			result.insert(src.get_direction(map_location::NORTH_EAST));
			result.insert(src.get_direction(map_location::NORTH_WEST));
		} else if(my_y > 0) {
			result.insert(src.get_direction(map_location::SOUTH));
			result.insert(src.get_direction(map_location::SOUTH_EAST));
			result.insert(src.get_direction(map_location::SOUTH_WEST));
		}
	} else {
		surface image2;
		if(!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
			image2 = image::get_image(image_loc);
		}
		if (image2 != NULL) {
			const double zoom_factor = disp->get_zoom_factor();
			int image_width = image::calculate_scaled_to_zoom(image2->w);
			int image_height = image::calculate_scaled_to_zoom(image2->h);
			bool facing_west = direction == map_location::NORTH_WEST || direction == map_location::SOUTH_WEST;
			bool facing_north = direction == map_location::NORTH_WEST || direction == map_location::NORTH || direction == map_location::NORTH_EAST;
			if(!current_data.auto_vflip) facing_north = true;
			if(!current_data.auto_hflip) facing_west = false;
			int my_x = current_data.x * zoom_factor + d2 - image_width/2;
			int my_y = current_data.y * zoom_factor + d2 - image_height/2;
			if(facing_west) {
				my_x += current_data.directional_x;
			} else {
				my_x -= current_data.directional_x;
			}
			if(facing_north) {
				my_y += current_data.directional_y;
			} else {
				my_y -= current_data.directional_y;
			}

			const int x = static_cast<int>(tmp_offset_x * xdst + (1.0-tmp_offset_x) * xsrc)+my_x;
			const int y = static_cast<int>(tmp_offset_y * ydst + (1.0-tmp_offset_y) * ysrc)+my_y;
			const SDL_Rect r = create_rect(x, y, image_width, image_height);
			// check if our underlying hexes are invalidated
			// if we need to update ourselve because we changed, invalidate our hexes
			// and return whether or not our hexs was invalidated
			// invalidate ouself to be called at redraw time
			result.insert(src);
			rect_of_hexes underlying_hex = disp->hexes_under_rect(r);
			result.insert(underlying_hex.begin(),underlying_hex.end());
		} else {
			// we have no "redraw surface" but we still need to invalidate our own hex
			// in case we have a halo and/or sound that needs a redraw
			// invalidate ouself to be called at redraw time
			result.insert(src);
			result.insert(dst);
		}
	}
	return result;
}

std::set<map_location> unit_frame::get_overlaped_hex_area_mode(const int frame_time, const frame_parameters& current_data) const
{
	display* disp = display::get_singleton();

	std::vector<SDL_Rect> rects = get_overlaped_rect_area_mode(frame_time, current_data);
	std::set<map_location> result;
	for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& r = *it;
		// check if our underlying hexes are invalidated
		// if we need to update ourselve because we changed, invalidate our hexes
		// and return whether or not our hexs was invalidated
		// invalidate ouself to be called at redraw time
		rect_of_hexes underlying_hex = disp->hexes_under_rect(r);
		result.insert(underlying_hex.begin(), underlying_hex.end());
	}

	return result;
}

std::vector<SDL_Rect> unit_frame::get_overlaped_rect_area_mode(const int frame_time, const map_location & src,const map_location & dst,const frame_parameters & animation_val,const frame_parameters & engine_val) const
{
	const frame_parameters current_data = merge_parameters(frame_time, animation_val, engine_val);
	return get_overlaped_rect_area_mode(frame_time, current_data);
}

std::vector<SDL_Rect> unit_frame::get_overlaped_rect_area_mode(const int frame_time, const frame_parameters& current_data) const
{
		// const frame_parameters current_data = merge_parameters(frame_time,animation_val,engine_val);
	double tmp_offset_x = current_data.offset_x;
	double tmp_offset_y = current_data.offset_y;

	display& disp = *display::get_singleton();
	const bool map = anim2::rt.type == anim_map;
	const int hdpi_scale = map? 1: gui2::twidget::hdpi_scale;
	image::locator image_loc = image::locator(current_data.image, current_data.image_mod);

	surface image;
	int image_width = 0, image_height= 0;
	if (!image_loc.is_void() && image_loc.get_filename() != "") { // invalid diag image, or not diagonal
		if (map) {
			image = image::get_image(image_loc);
			if (image.get() != NULL) {
				image_width = image::calculate_scaled_to_zoom(image->w);
				image_height = image::calculate_scaled_to_zoom(image->h);
			}
		} else {
			image = image::get_image(image_loc);
			if (image.get() != NULL) {
				image_width = image.get()->w * anim2::rt.zoomx;
				image_height = image.get()->h * anim2::rt.zoomy;
			}
		}
	}

	SDL_Rect map_area = !map? anim2::rt.rect: disp.main_map_widget_rect();

	double zoom_factor_x = !map? anim2::rt.zoomx: disp.get_zoom_factor();
	double zoom_factor_y = !map? anim2::rt.zoomy: zoom_factor_x;

	const int x = static_cast<int>(tmp_offset_x * map_area.w) + (!map? map_area.x: 0);
	const int y = static_cast<int>(tmp_offset_y * map_area.h) + (!map? map_area.y: 0);

	bool facing_west = false;
	bool facing_north = true;

	std::vector<SDL_Rect> result;
	if (image != NULL || !current_data.stext.empty()) {
		bool facing_west = false;
		bool facing_north = false;

		int w, h;
		if (image != NULL) {
			VALIDATE(image_width && image_height, null_str);
			w = image_width;
			h = image_height;
		} else {
			VALIDATE(map, "in area aimation, only anim_map support stext.");
			const size_t font_sz = static_cast<size_t>(current_data.font_size * disp.get_zoom_factor());
			surface	text_surf = font::get_rendered_text(current_data.stext, 0, current_data.font_size, font::NORMAL_COLOR);
			// why add +1, reference draw_text_in_hex2, there is outline.
			w = text_surf->w + 1;
			h = text_surf->h + 1;
		}

		int my_x = int(current_data.x * hdpi_scale * zoom_factor_x - w / 2);
		int my_y = int(current_data.y * hdpi_scale * zoom_factor_y - h / 2);

		if (map && (zero_x_ != nposm && zero_y_ != nposm)) {
			const map_location zero_loc(0,0);
			int zero_x = disp.loc_2_screen_x(zero_loc);
			int zero_y = disp.loc_2_screen_y(zero_loc);
			my_x += zero_x - zero_x_;
			my_y += zero_y - zero_y_;
		}

		int x2 = x + my_x;
		int y2 = y + my_y;
		if (!image) {
			x2 -= 1;
			y2 -= 1;
		}

		result.push_back(create_rect(x2, y2, w, h));
	}

	return result;
}

void unit_frame::replace_image_name(const std::string& src, const std::string& dst)
{
	if (builder_.image_ == src) {
		builder_.image_ = dst;
	}
}

void unit_frame::replace_image_mod(const std::string& src, const std::string& dst)
{
	if (builder_.image_mod_ == src) {
		builder_.image_mod_ = dst;
	}
}

void unit_frame::replace_progressive(const std::string& name, const std::string& src, const std::string& dst)
{
	if (name == "x") {
		if (builder_.x_.get_original() == src) {
			int duration = builder_.x_.duration();
			builder_.x_ = progressive_int(dst, duration);
		}
	} else if (name == "y") {
		if (builder_.y_.get_original() == src) {
			int duration = builder_.y_.duration();
			builder_.y_ = progressive_int(dst, duration);
		}
	} else if (name == "offset_x") {
		if (builder_.offset_x_.get_original() == src) {
			int duration = builder_.offset_x_.duration();
			builder_.offset_x_ = progressive_double(dst, duration);
		}
	} else if (name == "offset_y") {
		if (builder_.offset_y_.get_original() == src) {
			int duration = builder_.offset_y_.duration();
			builder_.offset_y_ = progressive_double(dst, duration);
		}
	} else if (name == "alpha") {
		if (builder_.highlight_ratio_.get_original() == src) {
			int duration = builder_.highlight_ratio_.duration();
			builder_.highlight_ratio_ = progressive_double(dst, duration);
		}
	}
}

void unit_frame::replace_static_text(const std::string& src, const std::string& dst)
{
	if (builder_.stext_ == src) {
		builder_.stext_ = dst;
	}
}

void unit_frame::replace_int(const std::string& name, int src, int dst)
{
	if (name == "duration") {
		if (builder_.duration_ == src) {
			builder_.duration_ = dst;
		}

	} else if (name == "text_color") {
		if (builder_.text_color_ == (src & 0xffffff)) {
			builder_.text_color_ = dst & 0xffffff;
		}
	} 
}


const frame_parameters unit_frame::merge_parameters(int current_time,const frame_parameters & animation_val,const frame_parameters & engine_val) const
{
	/**
	 * this function merges the value provided by
	 *  * the frame
	 *  * the engine (poison, flying unit...)
	 *  * the animation as a whole
	 *  there is no absolute rule for merging, so creativity is the rule
	 *  if a value is never provided by the engine, assert. (this way if it becomes used, people will easily find the right place to look)
	 *
	 */
	frame_parameters result;
	const frame_parameters & current_val = builder_.parameters(current_time);
	
	result.primary_frame = engine_val.primary_frame;
	if(animation_val.primary_frame != t_unset) result.primary_frame = animation_val.primary_frame;
	if(current_val.primary_frame != t_unset) result.primary_frame = current_val.primary_frame;
	const bool primary = result.primary_frame;

	/** engine provides a default image to use for the unit when none is available */
/*	result.image = current_val.image.is_void() || current_val.image.get_filename() == ""?animation_val.image:current_val.image;
	if(primary && ( result.image.is_void() || result.image.get_filename().empty())) {
		result.image = engine_val.image;
	}
*/
	result.image = current_val.image.empty()? animation_val.image: current_val.image;
	image::locator img(result.image);
	if (primary && (img.is_void() || img.get_filename().empty())) {
		result.image = engine_val.image;
	}

	/** engine provides a default image to use for the unit when none is available */
	result.image_diagonal = current_val.image_diagonal.is_void() || current_val.image_diagonal.get_filename() == ""?animation_val.image_diagonal:current_val.image_diagonal;
	if (primary && (result.image_diagonal.is_void() || result.image_diagonal.get_filename().empty())) {
		result.image_diagonal = engine_val.image_diagonal;
	}
	result.image_horizontal = current_val.image_horizontal.is_void() || current_val.image_horizontal.get_filename() == ""? animation_val.image_horizontal: current_val.image_horizontal;
	if (primary && (result.image_horizontal.is_void() || result.image_horizontal.get_filename().empty())) {
		result.image_horizontal = engine_val.image_horizontal;
	}

	/** engine provides a string for "petrified" and "team color" modifications
          note that image_mod is the complete modification and halo_mod is only the TC part
          see unit.cpp, we know that and use it*/
	result.image_mod = current_val.image_mod + animation_val.image_mod;
	if (result.primary_frame) {
		result.image_mod += engine_val.image_mod;
	}

	result.stext = current_val.stext.empty()? animation_val.stext: current_val.stext;

	assert(engine_val.halo.empty());
	result.halo = current_val.halo.empty()?animation_val.halo:current_val.halo;

	assert(engine_val.duration == 0);
	result.duration = current_val.duration;

	assert(engine_val.sound.empty());
	result.sound = current_val.sound.empty()?animation_val.sound:current_val.sound;

	assert(engine_val.text.empty());
	result.text = current_val.text.empty()?animation_val.text:current_val.text;

	assert(engine_val.text_color == 0);
	result.text_color = current_val.text_color?current_val.text_color:animation_val.text_color;

	result.font_size = current_val.font_size? current_val.font_size: animation_val.font_size;
	if (!result.font_size) {
		result.font_size = font::SIZE_DEFAULT;
	}

	/** engine provide a blend color for poisoned units */
	result.blend_with = current_val.blend_with?current_val.blend_with:animation_val.blend_with;
	if(primary&& engine_val.blend_with) result.blend_with = display::max_rgb(engine_val.blend_with,result.blend_with);

	/** engine provide a blend color for poisoned units */
	result.blend_ratio = current_val.blend_ratio?current_val.blend_ratio:animation_val.blend_ratio;
	if(primary && engine_val.blend_ratio) result.blend_ratio = std::min(result.blend_ratio + engine_val.blend_ratio, (double) 1.0);

	/** engine provide a highlight ratio for selected units and visible "invisible" units */
	result.highlight_ratio = current_val.highlight_ratio!=1.0?current_val.highlight_ratio:animation_val.highlight_ratio;
	if(primary && engine_val.highlight_ratio != 1.0) result.highlight_ratio = result.highlight_ratio +engine_val.highlight_ratio - 1.0; // selected unit

	assert(engine_val.offset_x == 0);
	result.offset_x = (current_val.offset_x!=-1000)?current_val.offset_x:animation_val.offset_x;
	if(result.offset_x == -1000) result.offset_x = 0.0;

	assert(engine_val.offset_y == 0);
	result.offset_y = (current_val.offset_y!=-1000)?current_val.offset_y:animation_val.offset_y;
	if(result.offset_y == -1000) result.offset_y = 0.0;

	/** engine provides a submerge for units in water */
	result.submerge = current_val.submerge?current_val.submerge:animation_val.submerge;
	if(primary && engine_val.submerge && !result.submerge ) result.submerge = engine_val.submerge;

	assert(engine_val.x == 0);
	result.x = current_val.x?current_val.x:animation_val.x;

	/** the engine provide y modification for terrain with height adjust and flying units */
	result.y = current_val.y?current_val.y:animation_val.y;
	result.y += engine_val.y;

	assert(engine_val.directional_x == 0);
	result.directional_x = current_val.directional_x?current_val.directional_x:animation_val.directional_x;
	assert(engine_val.directional_y == 0);
	result.directional_y = current_val.directional_y?current_val.directional_y:animation_val.directional_y;

	// assert(engine_val.drawing_layer == display::LAYER_UNIT_DEFAULT-display::LAYER_UNIT_FIRST);
	result.drawing_layer = current_val.drawing_layer !=  display::LAYER_UNIT_DEFAULT-display::LAYER_UNIT_FIRST?
		current_val.drawing_layer:animation_val.drawing_layer;
	if(primary&& engine_val.drawing_layer != result.drawing_layer) result.drawing_layer = engine_val.drawing_layer;

	/** the engine provide us with default value to compare with, we update if different */
	result.auto_hflip = engine_val.auto_hflip;
	if(animation_val.auto_hflip != t_unset) result.auto_hflip = animation_val.auto_hflip;
	if(current_val.auto_hflip != t_unset) result.auto_hflip = current_val.auto_hflip;
	if(result.auto_hflip == t_unset) result.auto_hflip = t_true;
	
	result.auto_vflip = engine_val.auto_vflip;
	if(animation_val.auto_vflip != t_unset) result.auto_vflip = animation_val.auto_vflip;
	if(current_val.auto_vflip != t_unset) result.auto_vflip = current_val.auto_vflip;
	if(result.auto_vflip == t_unset) {
		if(primary) result.auto_vflip=t_false;
		else result.auto_vflip = t_true;
	}

	result.area_mode = engine_val.area_mode;
	result.sound_filter = engine_val.sound_filter;

	return result;
}