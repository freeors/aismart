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

#include "gui/widgets/scroll_text_box.hpp"

#include "gui/widgets/text_box.hpp"
#include "gui/auxiliary/widget_definition/scroll_text_box.hpp"
#include "gui/auxiliary/window_builder/scroll_text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_text_box)

tscroll_text_box::tscroll_text_box()
	: tscroll_container(COUNT)
	, state_(ENABLED)
	, tb_(nullptr)
{
	unrestricted_drag_ = false;
}

void tscroll_text_box::mini_finalize_subclass()
{
	VALIDATE(content_grid_ && !tb_, null_str);
	tb_ = dynamic_cast<ttext_box*>(content_grid()->find("_text_box", false));

	tb_->set_restrict_width();
	tb_->set_text_font_size(text_font_size_);
	tb_->set_text_color_tpl(text_color_tpl_);

	tb_->set_did_text_changed(boost::bind(&tscroll_text_box::did_text_changed, this, _1));
	tb_->set_did_cursor_moved(boost::bind(&tscroll_text_box::did_cursor_moved, this, _1));
}

bool tscroll_text_box::mini_mouse_motion(const tpoint& first, const tpoint& last)
{
	if (tb_->selectioning()) {
		return false;
	}
	if (!tb_->start_selectioning()) {
		return true;
	}

	const int abs_diff_x = abs(last.x - first.x);
	const int abs_diff_y = abs(last.y - first.y);
	if (abs_diff_x >= settings::clear_click_threshold || abs_diff_x >= settings::clear_click_threshold) {
		tb_->cancel_start_selection();
		return true;
	}
	return false;
}

tpoint tscroll_text_box::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	tb_->set_text_maximum_width(content_size.x);

	tcalculate_text_box_lock lock(*tb_);
	const tpoint actual_size = content_grid_->get_best_size();

	return tpoint(std::max(actual_size.x, content_size.x), std::max(actual_size.y, content_size.y));
}

void tscroll_text_box::set_text_editable(bool editable)
{
	if (content_grid()) {
		ttext_box* widget = find_widget<ttext_box>(content_grid(), "_text_box", false, true);
		widget->set_text_editable(editable);
	}
}

const std::string& tscroll_text_box::label() const
{
	VALIDATE(tb_, null_str);
	return tb_->label();
}

void tscroll_text_box::set_label(const std::string& text)
{
	if (!tb_) {
		// tbuilder_control::init_control is called before finalize_setup.
		return;
	}
	tb_->set_label(text);
}

void tscroll_text_box::set_border(const std::string& border)
{ 
	tb_->set_border(border); 
}

void tscroll_text_box::set_atom_markup(const bool atom_markup)
{
	tb_->set_atom_markup(atom_markup);
}

void tscroll_text_box::set_placeholder(const std::string& label)
{ 
	tb_->set_placeholder(label);
}

void tscroll_text_box::set_default_color(const uint32_t color)
{
	tb_->set_default_color(color);
}

void tscroll_text_box::set_maximum_chars(const size_t maximum_chars)
{
	tb_->set_maximum_chars(maximum_chars);
}

void tscroll_text_box::insert_str(const std::string& str)
{
	tb_->insert_str(str);
}

void tscroll_text_box::insert_img(const std::string& str)
{
	tb_->insert_img(str);
}

void tscroll_text_box::did_text_changed(ttext_box& widget)
{
	if (did_text_changed_) {
		did_text_changed_(widget);
	}

	// scroll_label hasn't linked_group widget, to save time, don't calcuate linked_group.
	invalidate_layout(nullptr);
}

void tscroll_text_box::did_cursor_moved(ttext_box& widget)
{
	const unsigned cursor_at = widget.exist_selection()? widget.get_selection_end().y: widget.get_selection_start().y;
	const unsigned cursor_height = widget.cursor_height();
	const unsigned text_y_offset = widget.text_y_offset();
	unsigned item_position = vertical_scrollbar_->get_item_position();

	if (item_position > text_y_offset + cursor_at) {
		item_position = text_y_offset + cursor_at;
	} else if (item_position + get_height() < text_y_offset + cursor_at + cursor_height) {
		item_position = text_y_offset + cursor_at  + cursor_height - get_height();
	} else {
		return;
	}

	vertical_scrollbar_->set_item_position(item_position);
	scrollbar_moved();
}

const std::string& tscroll_text_box::get_control_type() const
{
	static const std::string type = "scroll_text_box";
	return type;
}

} // namespace gui2

