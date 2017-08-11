/* $Id$ */
/*
   Copyright (C) 2011 Sergey Popov <loonycyborg@gmail.com>
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

#include "gui/dialogs/progress.hpp"

#include "gettext.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, progress)

tprogress_* tprogress_::instance = nullptr;

tprogress::tprogress(const std::string& message, const boost::function<bool (tprogress_&)>& did_first_drawn, bool quantify, int hidden_ms, bool show_cancel, int best_width, int best_height)
	: message_(message)
	, start_ticks_(SDL_GetTicks())
	, did_first_drawn_(did_first_drawn)
	, quantify_(quantify)
	, hidden_ticks_(hidden_ms)
	, show_cancel_(show_cancel)
	, require_cancel_(false)
	, track_best_width_(best_width)
	, track_best_height_(best_height)
{
	VALIDATE(did_first_drawn_, null_str);
	VALIDATE(!quantify_ || hidden_ticks_ == 0, null_str);

	VALIDATE(tprogress_::instance == nullptr, "only allows a maximum of one tprogress");
}

tprogress::~tprogress()
{
	VALIDATE(!timer_.valid(), null_str);
	tprogress_::instance = nullptr;
}

void tprogress::pre_show()
{
	tprogress_::instance = this;

	window_->set_margin(0, 0, 0, 0);
	window_->set_border(null_str);

	progress_.reset(new tprogress_bar(*window_, find_widget<twidget>(window_, "_progress", false), message_, quantify_));
	progress_->track().set_best_size_1th(track_best_width_, track_best_height_);

	tbutton* button = find_widget<tbutton>(window_, "_cancel", false, false);
	if (!show_cancel_) {
		button->set_visible(twidget::INVISIBLE);
	}

	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tprogress::click_cancel
		, this));

	if (!quantify_) {
		window_->set_visible(twidget::INVISIBLE);
	}
}

void tprogress::post_show()
{
}

void tprogress::app_first_drawn()
{
	if (!quantify_) {
		// if block style, it will block during did_first_draw_, so must enable timer_ before it.
		timer_.reset(100, *get_window(), boost::bind(&tprogress::timer_handler, this));
	}

	bool ret = did_first_drawn_(*this);
	VALIDATE(!window_->is_closing(), null_str);

	if (ret) {
		VALIDATE(!require_cancel_, null_str);
		window_->set_retval(twindow::OK);
	} else {
		window_->set_retval(twindow::CANCEL);
	}
}

void tprogress::timer_handler()
{
	VALIDATE(!quantify_, null_str);

	if (window_->get_visible() != twidget::VISIBLE && SDL_GetTicks() - start_ticks_ >= hidden_ticks_) {
		window_->set_visible(twidget::VISIBLE);
		hidden_ticks_ = UINT32_MAX;
	}

	if (window_->get_visible() == twidget::VISIBLE) {
		progress_->track().timer_handler();
	}
}

void tprogress::set_percentage(const int percentage)
{
	VALIDATE(percentage >= 0 && percentage <= 100, null_str);

	bool changed = progress_->set_percentage(percentage);
	if (changed && quantify_) {
		absolute_draw();
	}
}

void tprogress::set_message(const std::string& message)
{
	bool changed = progress_->set_message(message);
	if (changed && quantify_) {
		absolute_draw();
	}
}

void tprogress::cancel_task()
{
	VALIDATE(!require_cancel_, null_str);
	require_cancel_ = true;
}

void tprogress::show_slice()
{ 
	window_->show_slice(); 
}

twidget::tvisible tprogress::get_visible() const
{
	return window_->get_visible();
}

void tprogress::set_visible(const twidget::tvisible visible)
{
	window_->set_visible(visible);
}

void tprogress::click_cancel()
{
	window_->set_retval(twindow::CANCEL);
}

bool run_with_progress(const std::string& message, const boost::function<bool (tprogress_&)>& did_first_drawn, bool quantify, int hidden_ms, bool show_cancel, const SDL_Rect& rect)
{
	gui2::tprogress dlg(message, did_first_drawn, quantify, hidden_ms, show_cancel, rect.w, rect.h);
	dlg.show(rect.x, rect.y);

	return dlg.get_retval() == twindow::OK;
}

} // namespace gui2

