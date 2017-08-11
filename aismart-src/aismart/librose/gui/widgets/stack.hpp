/* $Id: stack.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_STACK_HPP_INCLUDED
#define GUI_WIDGETS_STACK_HPP_INCLUDED

#include "gui/widgets/container.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_stack;
}

class tstack: public tcontainer_
{
	friend struct implementation::tbuilder_stack;

public:
	enum tmode {pip, radio, vertical, horizontal, modes};

	struct tupward_lock
	{
		tupward_lock(tstack& stack)
			: stack(stack)
		{
			VALIDATE(!stack.upward_, null_str);
			stack.upward_ = true;
		}
		~tupward_lock()
		{
			stack.upward_ = false;
		}

		tstack& stack;
	};

	class tgrid2: public tgrid
	{
		friend tstack;
	public:
		tgrid2(tstack& stack)
			: stack_(stack)
		{}

		tpoint calculate_best_size() const override;
		tpoint calculate_best_size_bh(const int width) override;
		void set_visible_area(const SDL_Rect& area) override;
		void set_origin(const tpoint& origin) override;
		void place(const tpoint& origin, const tpoint& size) override;

		twidget* find(const std::string& id, const bool must_be_active) override;
		const twidget* find(const std::string& id, const bool must_be_active) const override;

	private:
		void stack_init();
		void stack_insert_child(twidget& widget, int at, uint32_t factor);

	private:
		tstack& stack_;
	};

	class tgrid3: public tgrid
	{
	public:
		tgrid3(tstack& stack)
			: stack_(stack)
			, auto_draw_children_(true)
		{}

		void set_auto_draw_children(bool value) { auto_draw_children_ = value; }
		bool auto_draw_children() const { return auto_draw_children_; }
	private:
		/** Inherited from twidget. */
		void impl_draw_background(texture& frame_buffer, int x_offset, int y_offset);
		void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset);

	private:
		tstack& stack_;
		bool auto_draw_children_;
	};

	tstack();

	/** Inherited from tcontrol. */
	bool get_active() const { return true; }

	void set_mode(tmode mode) { mode_ = mode; }
	tmode mode() const { return mode_; }

	void set_radio_layer(int layer);

	tgrid* layer(int at) const;
	int layers() const { return grid_.children_vsize(); }

	//
	// vertical/horizontal
	//
	void set_splitter(twidget& splitter_bar, bool center_close = true);
	void set_did_can_drag(const boost::function<bool (tstack&, int x, int y)>& did) { did_can_drag_ = did; }
	void set_did_calculate_reserve(const boost::function<void (tstack&, int& max_top_movable, int& reserve_height, int& threshold)>& did) { did_calculate_reserve_ = did; }
	void set_did_mouse_event(const boost::function<void (tstack&, const tmouse_event event, const tpoint&, const tpoint&)>& did) { did_mouse_event_ = did; }
	bool upward() const { return upward_; }
	bool dragging() const { return !is_null_coordinate(first_coordinate_); }

private:
	//
	// vertical/horizontal
	//
	void signal_handler_left_button_down(const tpoint& coordinate);
	void signal_handler_mouse_leave(const tpoint& coordiante);
	void signal_handler_mouse_motion(const tpoint& last);
	void calculate_reserve();
	void move(const int diff_y);

	/** Inherited from tcontainer_ */
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack);

	/** Inherited from tcontrol. */
	unsigned get_state() const { return 0; }

	/** Inherited from tscroll_container. */
	void layout_children();

	void dirty_under_rect(const SDL_Rect& clip) override;

	/** Inherited from tcontrol. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;

	/**
	 * Finishes the building initialization of the widget.
	 *
	 * @param widget_builder      The builder to build the contents of the
	 *                            widget.
	 */
	void finalize(const std::vector<std::pair<tbuilder_grid_const_ptr, unsigned> >& widget_builder);

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/** Inherited from tcontainer_. */
	void set_self_active(const bool /*active*/) {}

private:
	/**
	 * Contains a pointer to the generator.
	 *
	 * The pointer is not owned by this class, it's stored in the content_grid_
	 * of the tscroll_container super class and freed when it's grid is
	 * freed.
	 */
	tgrid2 grid_;
	tmode mode_;

private:
	//
	// vertical/horizontal
	//
	bool center_close_;
	bool upward_;
	twidget* splitter_bar_;
	tpoint first_coordinate_;
	tpoint diff_;
	SDL_Rect std_top_rect_;
	SDL_Rect std_bottom_rect_;
	int max_top_movable_;
	int reserve_height_;
	int threshold_;
	tstack::tgrid3* top_grid_;
	tstack::tgrid3* bottom_grid_;
	boost::function<bool (tstack&, int x, int y)> did_can_drag_;
	boost::function<void (tstack&, int& max_top_movable, int& reserve_height, int& threshold)> did_calculate_reserve_;
	boost::function<void (tstack&, const tmouse_event event, const tpoint&, const tpoint&)> did_mouse_event_;
};

} // namespace gui2

#endif

