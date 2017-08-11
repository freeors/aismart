/* $Id: unit_frame.hpp 47268 2010-10-29 14:37:49Z boucman $ */
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

/**
 *  @file
 *  Frame for unit's animation sequence.
 */

#ifndef LIBROSE_UNIT_FRAME_H_INCLUDED
#define LIBROSE_UNIT_FRAME_H_INCLUDED

#include "image.hpp"

class config;

class progressive_string {
	public:
		progressive_string(const std::string& data = "",int duration = 0);
		int duration() const;
		const std::string & get_current_element(int time) const;
		bool does_not_change() const { return data_.size() <= 1; }
		std::string get_original() const { return input_; }
	private:
		std::vector<std::pair<std::string,int> > data_;
		std::string input_;
};

template <class T>
class progressive_
{
	std::vector<std::pair<std::pair<T, T>, int> > data_;
	std::string input_;
public:
	progressive_(const std::string& data = "", int duration = 0);
	int duration() const;
	const T get_current_element(int time,T default_val=0) const;
	bool does_not_change() const;
	std::string get_original() const { return input_; }
};

typedef progressive_<int> progressive_int;
typedef progressive_<double> progressive_double;

/** All parameters from a frame at a given instant */
class frame_parameters 
{
public:
	static const int default_text_color;

	frame_parameters();

	int duration;
	std::string image;
	image::locator image_diagonal;
	image::locator image_horizontal;
	std::string image_mod;
	std::string stext;
	std::string halo;
	std::string sound;
	std::string text;
	Uint32 text_color;
	int font_size;
	Uint32 blend_with;
	double blend_ratio;
	double highlight_ratio;
	double offset_x;
	double offset_y;
	double submerge;
	int x;
	int y;
	int directional_x;
	int directional_y;
	tristate auto_vflip;
	tristate auto_hflip;
	tristate primary_frame;
	int drawing_layer;
	bool area_mode;
	std::string sound_filter;

	static frame_parameters null_param;
};

namespace gui2 {
class tparticular;
}

/**
 * keep most parameters in a separate class to simplify handling of large
 * number of parameters hanling is common for frame level and animation level
 */
class frame_parsed_parameters 
{
public:
	frame_parsed_parameters(const config& cfg, const std::string& frame_string = "", int override_duration = 0);
	/** allow easy chained modifications will raised assert if used after initialization */
	void override( int duration
			, const std::string& highlight = ""
			, const std::string& blend_ratio =""
			, Uint32 blend_color = 0
			, const std::string& offset = ""
			, const std::string& layer = ""
			, const std::string& modifiers = "");
	/** getters for the different parameters */
	const frame_parameters parameters(int current_time) const ;

	int duration() const{ return duration_;};
	bool does_not_change() const;
	bool need_update() const;

	/** allow easy chained modifications will raised assert if used after initialization */
	frame_parsed_parameters& duration(const int duration);
	frame_parsed_parameters& image(const std::string& image ,const std::string & image_mod="");
	frame_parsed_parameters& image_diagonal(const image::locator& image_diagonal,const std::string & image_mod="");
	frame_parsed_parameters& image_horizontal(const image::locator& image_horizontal,const std::string & image_mod="");
	frame_parsed_parameters& sound(const std::string& sound);
	frame_parsed_parameters& text(const std::string& text,const  Uint32 text_color);
	frame_parsed_parameters& halo(const std::string &halo, const std::string& x, const std::string& y, const std::string& mod);
	frame_parsed_parameters& blend(const std::string& blend_ratio,const Uint32 blend_color);
	frame_parsed_parameters& highlight(const std::string& highlight);
	frame_parsed_parameters& offset(const std::string& offset);
	frame_parsed_parameters& offset(const std::string& offset_x, const std::string& offset_y);
	frame_parsed_parameters& submerge(const std::string& submerge);
	frame_parsed_parameters& x(const std::string& x);
	frame_parsed_parameters& y(const std::string& y);
	frame_parsed_parameters& directional_x(const std::string& directional_x);
	frame_parsed_parameters& directional_y(const std::string& directional_y);
	frame_parsed_parameters& auto_vflip(const bool auto_vflip);
	frame_parsed_parameters& auto_hflip(const bool auto_hflip);
	frame_parsed_parameters& primary_frame(const bool primary_frame);
	frame_parsed_parameters& drawing_layer(const std::string& drawing_layer);

private:
	friend class unit_frame;
	friend class tparticular;
	friend class gui2::tparticular;
	int duration_;
	std::string image_;
	image::locator image_diagonal_;
	image::locator image_horizontal_;
	std::string image_mod_;
	std::string stext_;
	progressive_string halo_;
	std::string sound_;
	std::string text_;
	Uint32 text_color_;
	int font_size_;
	Uint32 blend_with_;
	progressive_double blend_ratio_;
	progressive_double highlight_ratio_;
	progressive_double offset_x_;
	progressive_double offset_y_;
	progressive_double submerge_;
	progressive_int x_;
	progressive_int y_;
	progressive_int directional_x_;
	progressive_int directional_y_;
	tristate auto_vflip_;
	tristate auto_hflip_;
	tristate primary_frame_;
	progressive_int drawing_layer_;
};

/** Describe a unit's animation sequence. */
class unit_frame 
{
public:
	// Constructors
	unit_frame(const frame_parsed_parameters& builder = frame_parsed_parameters(config()))
		: builder_(builder)
		, zero_x_(nposm)
		, zero_y_(nposm)
	{}

	void redraw(const int frame_time,bool first_time,const map_location & src,const map_location & dst,int*halo_id,const frame_parameters & animation_val,const frame_parameters & engine_val)const;
	const frame_parameters merge_parameters(int current_time,const frame_parameters & animation_val,const frame_parameters & engine_val=frame_parameters()) const;
	const frame_parameters parameters(int current_time) const {return builder_.parameters(current_time);};

	int duration() const { return builder_.duration();};
	bool does_not_change() const{ return builder_.does_not_change();};
	bool need_update() const{ return builder_.need_update();};

	std::set<map_location> get_overlaped_hex(const int frame_time,const map_location & src,const map_location & dst,const frame_parameters & animation_val,const frame_parameters & engine_val) const;
	std::vector<SDL_Rect> get_overlaped_rect_area_mode(const int frame_time, const map_location & src,const map_location & dst,const frame_parameters & animation_val,const frame_parameters & engine_val) const;

	void replace_image_name(const std::string& src, const std::string& dst);
	void replace_image_mod(const std::string& src, const std::string& dst);
	void replace_progressive(const std::string& name, const std::string& src, const std::string& dst);
	void replace_static_text(const std::string& src, const std::string& dst);
	void replace_int(const std::string& name, int src, int dst);

	frame_parsed_parameters& get_builder() { return builder_; }
	const frame_parsed_parameters& get_builder() const { return builder_; }

private:
	void redraw_area_mode(const int frame_time, bool first_time, const map_location & src, const frame_parameters & current_data) const;
	std::set<map_location> get_overlaped_hex_area_mode(const int frame_time, const frame_parameters& current_data) const;
	std::vector<SDL_Rect> get_overlaped_rect_area_mode(const int frame_time, const frame_parameters& current_data) const;

private:
	frame_parsed_parameters builder_;
	mutable int zero_x_;
	mutable int zero_y_;
};

enum { ALIGN_NONE, ALIGN_X, ALIGN_NON_X, ALIGN_Y, ALIGN_NON_Y, ALIGN_COUNT};

#endif
