/* $Id: button.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TRACK_HPP_INCLUDED
#define GUI_WIDGETS_TRACK_HPP_INCLUDED

#include "gui/widgets/control.hpp"
#include "gui/widgets/timer.hpp"

namespace gui2 {

/**
 * Simple push button.
 */
class ttrack: public tcontrol
{
public:
	class tdraw_lock
	{
	public:
		explicit tdraw_lock(SDL_Renderer* renderer, ttrack& widget);
		~tdraw_lock();

	private:
		SDL_Renderer* renderer_;
		SDL_Texture* original_;
		SDL_Rect original_clip_rect_;
		ttrack& widget_;
	};

	ttrack();
	~ttrack();

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool active) override { active_ = active; }
		
	/** Inherited from tcontrol. */
	bool get_active() const override { return active_; }

	void set_did_draw(const boost::function<void (ttrack&, const SDL_Rect&, const bool)>& did)
	{ did_draw_ = did; }

	void set_did_mouse_motion(const boost::function<void (ttrack&, const tpoint&, const tpoint&)>& did)
	{ did_mouse_motion_ = did; }
	void set_did_left_button_down(const boost::function<void (ttrack&, const tpoint&)>& did)
	{ did_left_button_down_ = did; }
	void set_did_mouse_leave(const boost::function<void (ttrack&, const tpoint&, const tpoint&)>& did)
	{ did_mouse_leave_ = did; }

	SDL_Rect get_draw_rect() const;
	texture& background_texture() { return background_tex_; }

	void set_require_capture(bool val) { require_capture_ = val; }
	void set_timer_interval(int interval);

	void timer_handler();

	// called by tscroll_container.
	void reset_background_texture(const texture& screen, const SDL_Rect& rect);

	void clear_texture() override;

private:
	/** Inherited from tcontrol. */
	void impl_draw_background(
			  texture& frame_buffer
			, int x_offset
			, int y_offset) override;

	texture get_canvas_tex() override;

	/** Inherited from tcontrol. */
	unsigned get_state() const override { return 0; }

private:
	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/
	void signal_handler_mouse_leave(const tpoint& coordinate);

	void signal_handler_left_button_down(const tpoint& coordinate);

	void signal_handler_mouse_motion(const tpoint& coordinate);

private:
	bool active_;
	ttimer timer_;
	texture background_tex_;
	bool require_capture_;
	int timer_interval_;

	boost::function<void (ttrack&, const SDL_Rect&, const bool)> did_draw_;

	tpoint first_coordinate_;
	boost::function<void (ttrack&, const tpoint& first, const tpoint& last)> did_mouse_motion_;
	boost::function<void (ttrack&, const tpoint& last)> did_left_button_down_;
	boost::function<void (ttrack&, const tpoint& first, const tpoint& last)> did_mouse_leave_;
};

} // namespace gui2

#endif

