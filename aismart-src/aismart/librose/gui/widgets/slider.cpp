/* $Id: slider.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/slider.hpp"

#include "formatter.hpp"
#include "gui/auxiliary/widget_definition/slider.hpp"
#include "gui/auxiliary/window_builder/slider.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "sound.hpp"
#include "wml_exception.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace gui2 {

REGISTER_WIDGET(slider)

static int distance(const int a, const int b)
{
	/**
	 * @todo once this works properly the assert can be removed and the code
	 * inlined.
	 */
	int result =  b - a;
	VALIDATE(result >= 0, null_str);
	return result;
}

tslider::tslider():
		tscrollbar_(false),
		best_slider_length_(0),
		minimum_value_(0),
		minimum_value_label_(),
		maximum_value_label_(),
		value_labels_()
{
	connect_signal<event::SDL_KEY_DOWN>(boost::bind(
		&tslider::signal_handler_sdl_key_down, this, _2, _3, _5));
	connect_signal<event::LEFT_BUTTON_UP>(boost::bind(
		&tslider::signal_handler_left_button_up, this, _2, _3));

}

tpoint tslider::calculate_best_size() const
{
	// Inherited.
	tpoint result = tcontrol::calculate_best_size();
	if (best_slider_length_ != 0) {

		// Override length.
		boost::intrusive_ptr<const tslider_definition::tresolution> conf =
			boost::dynamic_pointer_cast<const tslider_definition::tresolution>(config());

		VALIDATE(conf, null_str);

		result.x = conf->left_offset + best_slider_length_ + conf->right_offset;
	}

	return result;
}

void tslider::set_value(const int value)
{
	if (value == get_value()) {
		return;
	}

	if (value < minimum_value_) {
		set_value(minimum_value_);
	} else if (value > get_maximum_value()) {
		set_value(get_maximum_value());
	} else {
		set_item_position(distance(minimum_value_, value));
		if (did_value_changed_) {
			did_value_changed_(*this, value);
		}
	}
}

void tslider::set_minimum_value(const int minimum_value)
{
	if (minimum_value == minimum_value_) {
		return;
	}

	/** @todo maybe make it a VALIDATE. */
	VALIDATE(minimum_value < get_maximum_value(), null_str);

	const int value = get_value();
	const int maximum_value = get_maximum_value();
	minimum_value_ = minimum_value;

	// The number of items needs to include the begin and end so distance + 1.
	set_item_count(distance(minimum_value_, maximum_value) + 1);

	if(value < minimum_value_) {
		set_item_position(0);
	} else {
		set_item_position(minimum_value_ + value);
	}
}

void tslider::set_maximum_value(const int maximum_value)
{
	if (maximum_value == get_maximum_value()) {
		return;
	}

	/** @todo maybe make it a VALIDATE. */
	VALIDATE(minimum_value_ < maximum_value, null_str);

	const int value = get_value();

	// The number of items needs to include the begin and end so distance + 1.
	set_item_count(distance(minimum_value_, maximum_value) + 1);

	if(value > maximum_value) {
		set_item_position(get_maximum_value());
	} else {
		set_item_position(minimum_value_ + value);
	}
}

t_string tslider::get_value_label() const
{
	if(!value_labels_.empty()) {
		VALIDATE(value_labels_.size() == get_item_count(), null_str);
		return value_labels_[get_item_position()];
	} else if(!minimum_value_label_.empty()
			&& get_value() == get_minimum_value()) {
		return minimum_value_label_;
	} else if(!maximum_value_label_.empty()
			&& get_value() == get_maximum_value()) {
		return maximum_value_label_;
	} else {
		return t_string((formatter() << get_value()).str());
	}
}

void tslider::child_callback_positioner_moved()
{
	// sound::play_UI_sound(settings::sound_slider_adjust);

	if (did_value_changed_) {
		did_value_changed_(*this, minimum_value_ + get_item_position());
	}
}

int tslider::minimum_positioner_length() const
{
	boost::intrusive_ptr<const tslider_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tslider_definition::tresolution>(config());
	return conf->minimum_positioner_length;
}

int tslider::maximum_positioner_length() const
{
	boost::intrusive_ptr<const tslider_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tslider_definition::tresolution>(config());
	return conf->maximum_positioner_length;
}

int tslider::offset_before() const
{
	boost::intrusive_ptr<const tslider_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tslider_definition::tresolution>(config());
	return conf->left_offset;
}

int tslider::offset_after() const
{
	boost::intrusive_ptr<const tslider_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tslider_definition::tresolution>(config());
	return conf->right_offset;
}

bool tslider::on_positioner(const tpoint& coordinate) const
{
	// Note we assume the positioner is over the entire height of the widget.
	return coordinate.x >= static_cast<int>(get_positioner_offset())
		&& coordinate.x < static_cast<int>(get_positioner_offset() + get_positioner_length())
		&& coordinate.y > 0
		&& coordinate.y < static_cast<int>(get_height());
}

int tslider::on_bar(const tpoint& coordinate) const
{
	// Not on the widget, leave.
	if (coordinate.x > get_width() || coordinate.y > get_height()) {
		return 0;
	}

	// we also assume the bar is over the entire height of the widget.
	if (coordinate.x < get_positioner_offset()) {
		return -1;
	} else if (coordinate.x >get_positioner_offset() + get_positioner_length()) {
		return 1;
	} else {
		return 0;
	}
}

void tslider::update_canvas()
{
	// slider must be circle.
	const int diameter = h_;
	const int diff = ((int)positioner_length_ - diameter) / 2;

	int adjusted_offset = (int)positioner_offset_ + diff;
	if (adjusted_offset < 0 || positioner_offset_ == 0) {
		adjusted_offset = 0;
	} else if (adjusted_offset > (int)w_ - diameter || w_ - positioner_offset_ == positioner_length_) {
		adjusted_offset = w_ - diameter;
	}

	BOOST_FOREACH(tcanvas& tmp, canvas()) {
		tmp.set_variable("positioner_offset", variant(adjusted_offset / twidget::hdpi_scale));
		tmp.set_variable("positioner_length", variant(diameter / twidget::hdpi_scale));
		tmp.set_variable("text", variant(get_value_label()));
	}
	set_dirty();
}

const std::string& tslider::get_control_type() const
{
	static const std::string type = "slider";
	return type;
}

void tslider::handle_key_decrease(bool& handled)
{
	handled = true;

	scroll(tscrollbar_::ITEM_BACKWARDS);
}

void tslider::handle_key_increase(bool& handled)
{
	handled = true;

	scroll(tscrollbar_::ITEM_FORWARD);
}

void tslider::signal_handler_sdl_key_down(const event::tevent event
		, bool& handled
		, const SDL_Keycode key)
{
	if (key == SDLK_DOWN || key == SDLK_LEFT) {
		handle_key_decrease(handled);
	} else if (key == SDLK_UP || key == SDLK_RIGHT) {
		handle_key_increase(handled);
	} else {
		// Do nothing. Ignore other keys.
	}
}

void tslider::signal_handler_left_button_up(
		const event::tevent event, bool& handled)
{
	get_window()->keyboard_capture(this);

	handled = true;
}

} // namespace gui2

