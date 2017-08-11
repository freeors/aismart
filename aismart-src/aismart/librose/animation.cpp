/* $Id: unit_animation.cpp 47268 2010-10-29 14:37:49Z boucman $ */
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

#include "global.hpp"

#include "animation.hpp"

#include "display.hpp"
#include "base_unit.hpp"
#include "base_controller.hpp"
#include "halo.hpp"
#include "map.hpp"
#include "rose_config.hpp"

#include <boost/foreach.hpp>
#include <algorithm>

#include "animated_impl.hpp"

template class animated< image::locator >;
template class animated< image::tblit >;
// template class animated< std::string >;
template class animated< unit_frame >;

animation::animation(int start_time, const unit_frame & frame, const std::string& event, const int variation, const frame_parsed_parameters& builder) 
	: unit_anim_(start_time, builder)
	, sub_anims_()
	, type_(anim_map)
	, src_()
	, dst_()
	, invalidated_(false)
	, play_offscreen_(true)
	, area_mode_(false)
	, layer_(0)
	, cycles_(false)
	, started_(false)
	, overlaped_hex_()
{
	add_frame(frame.duration(), frame, !frame.does_not_change());
}

animation::animation(const config& cfg) 
	: unit_anim_(cfg, "")
	, sub_anims_()
	, type_(anim_map)
	, src_()
	, dst_()
	, invalidated_(false)
	, play_offscreen_(true)
	, area_mode_(false)
	, layer_(0)
	, cycles_(false)
	, started_(false)
	, overlaped_hex_()
{
	BOOST_FOREACH (const config::any_child &fr, cfg.all_children_range()) {
		VALIDATE(!fr.key.empty(), null_str);
		if (fr.key.find("_frame", fr.key.size() - 6) == std::string::npos) {
			continue;
		}
		if (sub_anims_.find(fr.key) != sub_anims_.end()) {
			continue;
		}
		sub_anims_[fr.key] = particular(cfg, fr.key.substr(0, fr.key.size() - 5));
	}

	play_offscreen_ = cfg["offscreen"].to_bool(true);

	area_mode_ = cfg["area_mode"].to_bool();
	layer_ = cfg["layer"].to_int(); // use to outer anination
}


void animation::particular::override(int start_time
		, int duration
		, const std::string& highlight
		, const std::string& blend_ratio
		, Uint32 blend_color
		, const std::string& offset
		, const std::string& layer
		, const std::string& modifiers)
{
	set_begin_time(start_time);
	if (frames_.size() == 1) {
		unit_frame& T = frames_[0].value_;
		T.get_builder().override(duration,highlight,blend_ratio,blend_color,offset,layer,modifiers);
		frames_[0].duration_ = duration;
	} else {
		parameters_.override(duration,highlight,blend_ratio,blend_color,offset,layer,modifiers);
	}

	if (get_animation_duration() < duration) {
		const unit_frame & last_frame = get_last_frame();
		add_frame(duration -get_animation_duration(), last_frame);
	} else if(get_animation_duration() > duration) {
		set_end_time(duration);
	}

}

bool animation::particular::need_update() const
{
	if (animated<unit_frame>::need_update()) return true;
	if (get_current_frame().need_update()) return true;
	if (parameters_.need_update()) return true;
	return false;
}

bool animation::particular::need_minimal_update() const
{
	if (get_current_frame_begin_time() != last_frame_begin_time_ ) {
		return true;
	}
	return false;
}

animation::particular::particular(const config& cfg, const std::string& frame_string)
	: animated<unit_frame>()
	, accelerate(true)
	, cycles(false)
	, parameters_(config())
	, halo_id_(0)
	, last_frame_begin_time_(0)
{
	config::const_child_itors range = cfg.child_range(frame_string + "frame");
	starting_frame_time_ = INT_MAX;
	if (cfg[frame_string + "start_time"].empty() && range.first != range.second) {
		BOOST_FOREACH (const config& frame, range) {
			starting_frame_time_ = std::min(starting_frame_time_, frame["begin"].to_int());
		}
	} else {
		starting_frame_time_ = cfg[frame_string + "start_time"];
	}

	cycles = cfg[frame_string + "cycles"].to_bool();

	BOOST_FOREACH (const config &frame, range) {
		unit_frame tmp_frame(frame);
		add_frame(tmp_frame.duration(),tmp_frame,!tmp_frame.does_not_change());
	}
	parameters_ = frame_parsed_parameters(cfg, frame_string, get_animation_duration());
	if (!parameters_.does_not_change()  ) {
			force_change();
	}
}

bool animation::need_update() const
{
	if (unit_anim_.need_update()) return true;
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		if(anim_itor->second.need_update()) return true;
	}
	return false;
}

bool animation::need_minimal_update() const
{
	if (!play_offscreen_) {
		return false;
	}
	if (unit_anim_.need_minimal_update()) return true;
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		if(anim_itor->second.need_minimal_update()) return true;
	}
	return false;
}

bool animation::animation_finished() const
{
	if (!unit_anim_.animation_finished()) return false;
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		if(!anim_itor->second.animation_finished()) return false;
	}
	return true;
}

bool animation::animation_finished_potential() const
{
	if (!unit_anim_.animation_finished_potential()) return false;
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		if(!anim_itor->second.animation_finished_potential()) return false;
	}
	return true;
}

void animation::update_last_draw_time()
{
	double acceleration = 1.0;
	unit_anim_.update_last_draw_time(acceleration);
	std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.update_last_draw_time(acceleration);
	}
}

int animation::get_end_time() const
{
	int result = unit_anim_.get_end_time();
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::max<int>(result,anim_itor->second.get_end_time());
	}
	return result;
}

int animation::get_begin_time() const
{
	int result = unit_anim_.get_begin_time();
	std::map<std::string,particular>::const_iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		result= std::min<int>(result,anim_itor->second.get_begin_time());
	}
	return result;
}

void animation::start_animation(int start_time
		, const map_location &src
		, const map_location &dst
		, bool cycles
		, const std::string& text
		, const Uint32 text_color
		, const bool accelerate)
{
	started_ = true;
	unit_anim_.accelerate = accelerate;
	src_ = src;
	dst_ = dst;
	unit_anim_.start_animation(start_time, cycles || unit_anim_.cycles);
	cycles_ = cycles || unit_anim_.cycles;

	// now only one particular support cycle.
	VALIDATE(!cycles_ || sub_anims_.empty(), null_str);

	if (!text.empty()) {
		particular crude_build;
		frame_parsed_parameters tmp(null_cfg);
		crude_build.add_frame(1, tmp);
		crude_build.add_frame(1, tmp.text(text, text_color),true);
		sub_anims_["_add_text"] = crude_build;
	}
	std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.accelerate = accelerate;
		anim_itor->second.start_animation(start_time, cycles || anim_itor->second.cycles);
		cycles_ = cycles_ || anim_itor->second.cycles;
	}
}

void animation::update_parameters(const map_location &src, const map_location &dst)
{
	src_ = src;
	dst_ = dst;
}
void animation::pause_animation()
{

	std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
	unit_anim_.pause_animation();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.pause_animation();
	}
}
void animation::restart_animation()
{

	std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
	unit_anim_.restart_animation();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.restart_animation();
	}
}
void animation::redraw(frame_parameters& value)
{

	invalidated_=false;
	overlaped_hex_.clear();
	std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
	value.primary_frame = t_true;
	unit_anim_.redraw(value,src_,dst_);
	value.primary_frame = t_false;
	for ( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.redraw( value,src_,dst_);
	}
}

void animation::redraw(texture&, const SDL_Rect&, bool snap_bg)
{
	VALIDATE(!snap_bg, null_str);

	frame_parameters params;
	params.area_mode = area_mode_;
	redraw(params);
}

void animation::require_release()
{
	if (callback_finish_) {
		callback_finish_(this);
	}
}

void animation::replace_image_name(const std::string& part, const std::string& src, const std::string& dst)
{
	if (part.empty()) {
		unit_anim_.replace_image_name(src, dst);
	} else {
		std::map<std::string, particular>::iterator it = sub_anims_.find(part + "_frame");
		if (it != sub_anims_.end()) {
			it->second.replace_image_name(src, dst);
		}
	}
}

void animation::replace_image_mod(const std::string& part, const std::string& src, const std::string& dst)
{
	if (part.empty()) {
		unit_anim_.replace_image_mod(src, dst);
	} else {
		std::map<std::string, particular>::iterator it = sub_anims_.find(part + "_frame");
		if (it != sub_anims_.end()) {
			it->second.replace_image_mod(src, dst);
		}
	}
}

void animation::replace_progressive(const std::string& part, const std::string& name, const std::string& src, const std::string& dst)
{
	if (part.empty()) {
		unit_anim_.replace_progressive(name, src, dst);
	} else {
		std::map<std::string, particular>::iterator it = sub_anims_.find(part + "_frame");
		if (it != sub_anims_.end()) {
			it->second.replace_progressive(name, src, dst);
		}
	}
}

void animation::replace_static_text(const std::string& part, const std::string& src, const std::string& dst)
{
	if (part.empty()) {
		unit_anim_.replace_static_text(src, dst);
	} else {
		std::map<std::string, particular>::iterator it = sub_anims_.find(part + "_frame");
		if (it != sub_anims_.end()) {
			it->second.replace_static_text(src, dst);
		}
	}
}

void animation::replace_int(const std::string& part, const std::string& name, int src, int dst)
{
	if (part.empty()) {
		unit_anim_.replace_int(name, src, dst);
	} else {
		std::map<std::string, particular>::iterator it = sub_anims_.find(part + "_frame");
		if (it != sub_anims_.end()) {
			it->second.replace_int(name, src, dst);
		}
	}
}

void animation::clear_haloes()
{

	std::map<std::string,particular>::iterator anim_itor = sub_anims_.begin();
	unit_anim_.clear_halo();
	for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
		anim_itor->second.clear_halo();
	}
}
bool animation::invalidate(const int, const int, frame_parameters& value)
{
	if(invalidated_) return false;
	display* disp = display::get_singleton();
	bool complete_redraw =disp->tile_nearly_on_screen(src_) || disp->tile_nearly_on_screen(dst_) || !src_.valid();
	if (overlaped_hex_.empty()) {
		if (complete_redraw) {
			std::map<std::string,particular>::iterator anim_itor =sub_anims_.begin();
			value.primary_frame = t_true;
			overlaped_hex_ = unit_anim_.get_overlaped_hex(value,src_,dst_);
			value.primary_frame = t_false;
			for( /*null*/; anim_itor != sub_anims_.end() ; ++anim_itor) {
				std::set<map_location> tmp = anim_itor->second.get_overlaped_hex(value,src_,dst_);
				overlaped_hex_.insert(tmp.begin(),tmp.end());
			}
		} else {
			// off screen animations only invalidate their own hex, no propagation,
			// but we stil need this to play sounds
			overlaped_hex_.insert(src_);
		}

	}
	if (complete_redraw) {
		if( need_update()) {
			disp->invalidate(overlaped_hex_);
			invalidated_ = true;
			return true;
		} else {
			invalidated_ = disp->propagate_invalidation(overlaped_hex_);
			return invalidated_;
		}
	} else {
		if (need_minimal_update()) {
			disp->invalidate(overlaped_hex_);
			invalidated_ = true;
			return true;
		} else {
			return false;
		}
	}
}

bool animation::invalidate(const int fb_width, const int fb_height)
{
	frame_parameters params;
	params.area_mode = area_mode_;
	return invalidate(fb_width, fb_height, params);
}

void animation::particular::redraw(const frame_parameters& value,const map_location &src, const map_location &dst)
{
	if (get_animation_time() < get_begin_time()) {
		return;
	} else if (get_animation_time() > get_end_time()) {
		// return;
	}

	const unit_frame& current_frame= get_current_frame();
	const frame_parameters default_val = parameters_.parameters(get_animation_time() - get_begin_time());
	if(get_current_frame_begin_time() != last_frame_begin_time_ ) {
		last_frame_begin_time_ = get_current_frame_begin_time();
		current_frame.redraw(get_current_frame_time(),true,src,dst,&halo_id_,default_val,value);
	} else {
		current_frame.redraw(get_current_frame_time(),false,src,dst,&halo_id_,default_val,value);
	}
}

void animation::particular::replace_image_name(const std::string& src, const std::string& dst)
{
	for (std::vector<frame>::iterator fr = frames_.begin(); fr != frames_.end(); ++ fr) {
		unit_frame& T = fr->value_;
		T.replace_image_name(src, dst);
	}
}

void animation::particular::replace_image_mod(const std::string& src, const std::string& dst)
{
	for (std::vector<frame>::iterator fr = frames_.begin(); fr != frames_.end(); ++ fr) {
		unit_frame& T = fr->value_;
		T.replace_image_mod(src, dst);
	}
}

void animation::particular::replace_progressive(const std::string& name, const std::string& src, const std::string& dst)
{
	does_not_change_ = true;
	for (std::vector<frame>::iterator fr = frames_.begin(); fr != frames_.end(); ++ fr) {
		unit_frame& T = fr->value_;
		T.replace_progressive(name, src, dst);
		if (does_not_change_) {
			does_not_change_ = T.does_not_change();
		}
	}
}

void animation::particular::replace_static_text(const std::string& src, const std::string& dst)
{
	for (std::vector<frame>::iterator fr = frames_.begin(); fr != frames_.end(); ++ fr) {
		unit_frame& T = fr->value_;
		T.replace_static_text(src, dst);
	}
}

void animation::particular::replace_int(const std::string& name, int src, int dst)
{
	for (std::vector<frame>::iterator fr = frames_.begin(); fr != frames_.end(); ++ fr) {
		unit_frame& T = fr->value_;
		T.replace_int(name, src, dst);
	}
}

void animation::particular::clear_halo()
{
	if(halo_id_ != halo::NO_HALO) {
		halo::remove(halo_id_);
		halo_id_ = halo::NO_HALO;
	}
}
std::set<map_location> animation::particular::get_overlaped_hex(const frame_parameters& value,const map_location &src, const map_location &dst)
{
	const unit_frame& current_frame= get_current_frame();
	const frame_parameters default_val = parameters_.parameters(get_animation_time() -get_begin_time());
	return current_frame.get_overlaped_hex(get_current_frame_time(), src, dst, default_val, value);
}

std::vector<SDL_Rect> animation::particular::get_overlaped_rect(const frame_parameters& value,const map_location &src, const map_location &dst)
{
	const unit_frame& current_frame = get_current_frame();
	const frame_parameters default_val = parameters_.parameters(get_animation_time() - get_begin_time());
	return current_frame.get_overlaped_rect_area_mode(get_current_frame_time(), src, dst, default_val, value);
}

animation::particular::~particular()
{
	halo::remove(halo_id_);
	halo_id_ = halo::NO_HALO;
}

void animation::particular::start_animation(int start_time, bool cycles)
{
	halo::remove(halo_id_);
	halo_id_ = halo::NO_HALO;
	parameters_.override(get_animation_duration());
	animated<unit_frame>::start_animation(start_time,cycles);
	last_frame_begin_time_ = get_begin_time() -1;
}


void base_animator::add_animation2(base_unit* animated_unit
		, const animation* anim
		, const map_location &src
		, bool with_bars
		, bool cycles
		, const std::string& text
		, const Uint32 text_color)
{
	if (!animated_unit) {
		return;
	}

	anim_elem tmp;
	tmp.my_unit = animated_unit;
	tmp.text = text;
	tmp.text_color = text_color;
	tmp.src = src;
	tmp.with_bars= with_bars;
	tmp.cycles = cycles;
	tmp.anim = anim;
	if (!tmp.anim) {
		return;
	}

	start_time_ = std::max<int>(start_time_, tmp.anim->get_begin_time());
	animated_units_.push_back(tmp);
}

void base_animator::start_animations()
{
	int begin_time = INT_MAX;
	std::vector<anim_elem>::iterator anim;
	
	for (anim = animated_units_.begin(); anim != animated_units_.end(); ++ anim) {
		if (anim->my_unit->get_animation()) {
			if(anim->anim) {
				begin_time = std::min<int>(begin_time, anim->anim->get_begin_time());
			} else  {
				begin_time = std::min<int>(begin_time, anim->my_unit->get_animation()->get_begin_time());
			}
		}
	}

	for (anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		if (anim->anim) {
			// TODO: start_tick_ of particular is relative to last_update_tick_(reference to animated<T,T_void_value>::start_animation), 
			//       and last_update_tick generated by new_animation_frame().
			//       On the other hand, new_animation_frame() may be delayed, for example display gui2::dialog,
			//       once it occur, last_update_tick will indicate "past" time, result to start_tick_ "too early".
			//       ----Should displayed frame in this particular will not display!
			//       there need call new_animation_frame(), update last_update_tick to current tick.
			new_animation_frame();

			anim->my_unit->start_animation(begin_time, anim->anim,
				anim->with_bars, anim->cycles, anim->text, anim->text_color);
			anim->anim = NULL;
		} else {
			anim->my_unit->get_animation()->update_parameters(anim->src, anim->src.get_direction(anim->my_unit->facing()));
		}

	}
}

bool base_animator::would_end() const
{
	bool finished = true;
	for(std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		finished &= anim->my_unit->get_animation()->animation_finished_potential();
	}
	return finished;
}
void base_animator::wait_until(base_controller& controller, int animation_time) const
{
	display& disp = *display::get_singleton();

	double speed = disp.turbo_speed();
	controller.play_slice(false);
	int end_tick = animated_units_[0].my_unit->get_animation()->time_to_tick(animation_time);
	while (SDL_GetTicks() < static_cast<unsigned int>(end_tick)
				- std::min<int>(static_cast<unsigned int>(20/speed),20)) {

		disp.delay(std::max<int>(0,
			std::min<int>(10,
			static_cast<int>((animation_time - get_animation_time()) * speed))));
		controller.play_slice(false);
        end_tick = animated_units_[0].my_unit->get_animation()->time_to_tick(animation_time);
	}
	disp.delay(std::max<int>(0,end_tick - SDL_GetTicks() +5));
	new_animation_frame();
}

void base_animator::wait_for_end(base_controller& controller) const
{
	display& disp = *display::get_singleton();

	if (game_config::no_delay) return;
	bool finished = false;
	while(!finished) {
		controller.play_slice(false);
		disp.delay(10);
		finished = true;
		for (std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
			finished &= anim->my_unit->get_animation()->animation_finished_potential();
		}
	}
}
int base_animator::get_animation_time() const
{
	return animated_units_[0].my_unit->get_animation()->get_animation_time() ;
}

int base_animator::get_animation_time_potential() const
{
	return animated_units_[0].my_unit->get_animation()->get_animation_time_potential() ;
}

int base_animator::get_end_time() const
{
	int end_time = INT_MIN;
	for (std::vector<anim_elem>::const_iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		if(anim->my_unit->get_animation()) {
			end_time = std::max<int>(end_time,anim->my_unit->get_animation()->get_end_time());
		}
	}
	return end_time;
}
void base_animator::pause_animation()
{
        for(std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
	       if(anim->my_unit->get_animation()) {
                anim->my_unit->get_animation()->pause_animation();
	       }
        }
}
void base_animator::restart_animation()
{
    for (std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		if (anim->my_unit->get_animation()) {
			anim->my_unit->get_animation()->restart_animation();
		}
	}
}
void base_animator::set_all_standing()
{
	for(std::vector<anim_elem>::iterator anim = animated_units_.begin(); anim != animated_units_.end();++anim) {
		anim->my_unit->set_standing(true);
    }
}


static image::tblit null_blit;

tdrawing_buffer::~tdrawing_buffer()
{
	clear_area_anims();
}

void tdrawing_buffer::drawing_buffer_add(const tdrawing_layer layer,
		const map_location& loc, const surface& surf, const int x, const int y, const int width, const int height,
		const SDL_Rect &clip)
{
	if (!surf) {
		return;
	}

	std::list<tblit2>& drawing_buffer = drawing_buffer_;
	drawing_buffer.push_back(tblit2(layer, loc, 0, 0, image::tblit(surf, x, y, width, height, clip)));
}

image::tblit& tdrawing_buffer::drawing_buffer_add(const tdrawing_layer layer,
		const map_location& loc, const image::locator& loc2, const image::TYPE loc2_type, const int x, const int y, const int width, const int height,
		const SDL_Rect &clip)
{
	if (loc2.is_void()) {
		return null_blit;
	}
	// VALIDATE(!loc2.is_void(), null_str);

	std::list<tblit2>& drawing_buffer = drawing_buffer_;
	drawing_buffer.push_back(tblit2(layer, loc, 0, 0, image::tblit(loc2, loc2_type, x, y, width, height, clip)));
	return drawing_buffer.back().surf().back();
}

image::tblit& tdrawing_buffer::drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, const int type, const int x, const int y, const int width, const int height, const uint32_t color)
{
	VALIDATE(type == image::BLITM_RECT || image::BLITM_FRAME || type == image::BLITM_LINE, null_str);
	std::list<tblit2>& drawing_buffer = drawing_buffer_;
	drawing_buffer.push_back(tblit2(layer, loc, 0, 0, image::tblit(type, x, y, width, height, color)));
	return drawing_buffer.back().surf().back();
}

void tdrawing_buffer::drawing_buffer_add(const tdrawing_layer layer,
		const map_location& loc, int x, int y,
		const std::vector<image::tblit>& blits)
{
	std::list<tblit2>& drawing_buffer = drawing_buffer_;
	drawing_buffer.push_back(tblit2(layer, loc, x, y, blits));
}

// FIXME: temporary method. Group splitting should be made
// public into the definition of tdrawing_layer
//
// The drawing is done per layer_group, the range per group is [low, high).
const tdrawing_buffer::tdrawing_layer tdrawing_buffer::drawing_buffer_key::layer_groups[] = {
	LAYER_TERRAIN_BG,
	LAYER_UNIT_FIRST,
	LAYER_UNIT_MOVE_DEFAULT,
	// Make sure the movement doesn't show above fog and reachmap.
	LAYER_REACHMAP,
	LAYER_LAST_LAYER
};

// no need to change this if layer_groups above is changed
const unsigned int tdrawing_buffer::drawing_buffer_key::max_layer_group = sizeof(tdrawing_buffer::drawing_buffer_key::layer_groups) / sizeof(display::tdrawing_layer) - 2;

enum {
	// you may adjust the following when needed:

	// maximum border. 3 should be safe even if a larger border is in use somewhere
	MAX_BORDER           = 3,

	// store x, y, and layer in one 32 bit integer
	// 4 most significant bits == layer group   => 16
	BITS_FOR_LAYER_GROUP = 4,

	// 10 second most significant bits == y     => 1024
	BITS_FOR_Y           = 10,

	// 1 third most significant bit == x parity => 2
	BITS_FOR_X_PARITY    = 1,

	// 8 fourth most significant bits == layer   => 256
	BITS_FOR_LAYER       = 8,

	// 9 least significant bits == x / 2        => 512 (really 1024 for x)
	BITS_FOR_X_OVER_2    = 9
};

inline tdrawing_buffer::drawing_buffer_key::drawing_buffer_key(const map_location &loc, tdrawing_layer layer)
	: key_(0)
{
	// max_layer_group + 1 is the last valid entry in layer_groups, but it is always > layer
	// thus the first --g is a given => start with max_layer_groups right away
	unsigned int g = max_layer_group;
	while (layer < layer_groups[g]) {
		--g;
	}

	enum {
		SHIFT_LAYER          = BITS_FOR_X_OVER_2,
		SHIFT_X_PARITY       = BITS_FOR_LAYER + SHIFT_LAYER,
		SHIFT_Y              = BITS_FOR_X_PARITY + SHIFT_X_PARITY,
		SHIFT_LAYER_GROUP    = BITS_FOR_Y + SHIFT_Y
	};
	BOOST_STATIC_ASSERT(SHIFT_LAYER_GROUP + BITS_FOR_LAYER_GROUP == sizeof(key_) * 8);

	// the parity of x must be more significant than the layer but less significant than y.
	// Thus basically every row is split in two: First the row containing all the odd x
	// then the row containing all the even x. Since thus the least significant bit of x is
	// not required for x ordering anymore it can be shifted out to the right.
	const unsigned int x_parity = static_cast<unsigned int>(loc.x) & 1;
	key_  = (g << SHIFT_LAYER_GROUP) | (static_cast<unsigned int>(loc.y + MAX_BORDER) << SHIFT_Y);
	key_ |= (x_parity << SHIFT_X_PARITY);
	key_ |= (static_cast<unsigned int>(layer) << SHIFT_LAYER) | static_cast<unsigned int>(loc.x + MAX_BORDER) / 2;
}

void tdrawing_buffer::drawing_buffer_commit(texture& screen, const SDL_Rect& clip_rect)
{
/*
	SDL_BlendMode blend_mode;
	SDL_GetSurfaceBlendMode(screen, &blend_mode);
	VALIDATE(blend_mode == SDL_BLENDMODE_NONE, null_str);
*/

	SDL_Renderer* renderer = get_renderer();
	texture_clip_rect_setter clip(&clip_rect);

	std::list<tblit2>& drawing_buffer = drawing_buffer_;
	// std::list::sort() is a stable sort
	uint32_t start = SDL_GetTicks();
	drawing_buffer.sort();
	uint32_t ticks1 = SDL_GetTicks();

	/*
	 * Info regarding the rendering algorithm.
	 *
	 * In order to render a hex properly it needs to be rendered per row. On
	 * this row several layers need to be drawn at the same time. Mainly the
	 * unit and the background terrain. This is needed since both can spill
	 * in the next hex. The foreground terrain needs to be drawn before to
	 * avoid decapitation a unit.
	 *
	 * This ended in the following priority order:
	 * layergroup > location > layer > 'tblit' > surface
	 */

	BOOST_FOREACH (const tblit2 &blit3, drawing_buffer) {
		const std::vector<image::tblit>& blits = blit3.surf();
		BOOST_FOREACH (const image::tblit& blit, blits) {
			image::render_blit(renderer, blit, blit3.x(), blit3.y());
		}
	}
	// SDL_Log("drawing_buffer_commit, lists: %u, sort: %u\n", drawing_buffer.size(), ticks1 - start);
	drawing_buffer.clear();
}

animation& tdrawing_buffer::area_anim(int id)
{
	return *area_anims_.find(id)->second;
}

int tdrawing_buffer::insert_area_anim(const animation& tpl)
{
	animation* clone = new animation(tpl);
	return insert_area_anim2(clone);
}

int tdrawing_buffer::insert_area_anim2(animation* anim)
{
	int id = -1;
	for (std::map<int, animation*>::reverse_iterator it = area_anims_.rbegin(); it != area_anims_.rend(); ++ it) {
		id = it->first;
		break;
	}
	id ++;
	area_anims_.insert(std::make_pair(id, anim));
	return id;
}

void tdrawing_buffer::erase_area_anim(int id)
{
	std::map<int, animation*>::iterator find = area_anims_.find(id);
	VALIDATE(find != area_anims_.end(), "erase_area_anim, cannot find 'id' screen anim!");
	VALIDATE(find->second->type() != anim_map, null_str);

	// find->second->undraw(video_.getSurface());
	delete find->second;
	area_anims_.erase(find);
}

void tdrawing_buffer::clear_area_anims()
{
	for (std::map<int, animation*>::const_iterator it = area_anims_.begin(); it != area_anims_.end(); ++ it) {
		delete it->second;
	}
	area_anims_.clear();
}


void tdrawing_buffer::draw_text_in_hex2(const map_location& loc,
		const tdrawing_layer layer, const std::string& text,
		size_t font_size, SDL_Color color, int x, int y, fixed_t alpha, bool center)
{
	if (text.empty()) return;

	const size_t font_sz = static_cast<size_t>(font_size * get_zoom_factor());

	surface	text_surf = font::get_rendered_text(text, 0, font_sz, color);
	surface	back_surf = font::get_rendered_text(text, 0, font_sz, font::BLACK_COLOR);

	if (center) {
		x = x - text_surf->w/2;
		y = y - text_surf->h/2;
	}

	for (int dy=-1; dy <= 1; ++dy) {
		for (int dx=-1; dx <= 1; ++dx) {
			if (dx!=0 || dy!=0) {
				drawing_buffer_add(layer, loc, back_surf, x + dx, y + dy, 0, 0);
			}
		}
	}

	drawing_buffer_add(layer, loc, text_surf, x, y, 0, 0);
}

void tdrawing_buffer::render_image(int x, int y, const tdrawing_layer drawing_layer,
		const map_location& loc, image::tblit& image,
		bool hreverse, bool greyscale, fixed_t alpha,
		Uint32 blendto, double blend_ratio, double submerged, bool vreverse)
{

	VALIDATE(image.surf.get() && image.width && image.height && !greyscale, null_str);

	if (hreverse) {
		image.flip |= SDL_FLIP_HORIZONTAL;
	}
	if (vreverse) {
		image.flip |= SDL_FLIP_VERTICAL;
	}
	if (blend_ratio != 0) {
		image.blend_ratio = blend_ratio * 255;
		image.blend_color = blendto;
	}
	if (alpha > ftofxp(1.0)) {
		image.modulation_alpha = alpha;
	} else if(alpha != ftofxp(1.0)) {
		image.modulation_alpha = alpha;
	}

	if (submerged > 0.0) {
		// divide the surface into 2 parts
		const int unsubmerge_height = std::max<int>(0, image.height * (1.0 - submerged));
		// const int depth = surf->h - submerge_height;
		SDL_Rect srcrect = create_rect(0, 0, image.width, unsubmerge_height);
		image::tblit& material2 = drawing_buffer_add(drawing_layer, loc, *image.loc, image.loc_type, x, y, 0, 0, srcrect);
		material2.height = image::calculate_scaled_to_zoom(srcrect.h);

		if (unsubmerge_height != image.height) {
			//the lower part will be transparent
			srcrect.y = unsubmerge_height;
			srcrect.h = image.height - unsubmerge_height;
			y += unsubmerge_height;

			image::tblit& material2 = drawing_buffer_add(drawing_layer, loc, *image.loc, image.loc_type, x, y, 0, 0, srcrect);
			material2.height = image::calculate_scaled_to_zoom(srcrect.h);
			material2.modulation_alpha = 255 * 0.3;
		}
	} else {
		// simple blit
		image::tblit& material2 = drawing_buffer_add(drawing_layer, loc, *image.loc, image.loc_type, x, y, 0, 0);
		material2.flip = image.flip;
		material2.modulation_alpha = image.modulation_alpha;
		material2.blend_ratio = image.blend_ratio;
		material2.blend_color = image.blend_color;
		material2.width = image.width;
		material2.height = image.height;
	}
}