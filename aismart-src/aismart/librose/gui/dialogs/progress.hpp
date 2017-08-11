/* $Id$ */
/*
   Copyright (C) 2011 by Sergey Popov <loonycyborg@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_PROGRESS_HPP_INCLUDED
#define GUI_DIALOGS_PROGRESS_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/progress_bar.hpp"
#include "gui/widgets/timer.hpp"

namespace gui2 {

class tbutton;

/**
 * Dialog that tracks network transmissions
 *
 * It shows upload/download progress and allows the user
 * to cancel the transmission.
 */
class tprogress : public tdialog, public tprogress_
{
public:
	tprogress(const std::string& message, const boost::function<bool (tprogress_&)>& did_first_drawn, bool quantify, int hidden_ms, bool show_cancel, int best_width, int best_height);
	~tprogress();

	void set_percentage(const int percentage) override;
	void set_message(const std::string& message) override;
	void cancel_task() override;
	bool task_canceled() const override { return require_cancel_; }
	void show_slice() override;
	twidget::tvisible get_visible() const override;
	void set_visible(const twidget::tvisible visible) override;

protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_cancel();

private:
	void app_first_drawn() override;
	void timer_handler();

private:
	// The title for the dialog.
	const std::string message_;
	const boost::function<bool (tprogress_&)> did_first_drawn_;
	const bool quantify_;
	Uint32 hidden_ticks_;
	const bool show_cancel_;

	std::unique_ptr<tprogress_bar> progress_;
	
	Uint32 start_ticks_;
	bool require_cancel_;
	const int track_best_width_;
	const int track_best_height_;

	ttimer timer_;
};

} // namespace gui2

#endif

