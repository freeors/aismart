/* $Id: container.hpp 54906 2012-07-29 19:52:01Z mordante $ */
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

#ifndef GUI_WIDGETS_CONTAINER_HPP_INCLUDED
#define GUI_WIDGETS_CONTAINER_HPP_INCLUDED

#include "gui/widgets/grid.hpp"
#include "gui/widgets/control.hpp"
#include "gui/auxiliary/window_builder.hpp"

namespace gui2 {

/**
 * A generic container base class.
 *
 * A container is a class build with multiple items either acting as one
 * widget.
 *
 */
class tcontainer_ : public tcontrol
{
public:
	explicit tcontainer_(const unsigned canvas_count)
		: tcontrol(canvas_count)
		, grid_(nullptr)
		, explicit_margin_({nposm, nposm, nposm, nposm})
	{
		terminal_ = false;
	}

	/**
	 * Returns the size of the client area.
	 *
	 * The client area is the area available for widgets.
	 */
	SDL_Rect get_grid_rect() const;

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void layout_init(bool linked_group_only) override;

	virtual const tgrid& layout_grid() const { return *grid_; }

protected:
	/** Inherited from twidget. */
	tpoint calculate_best_size() const override;

public:
	/** Inherited from twidget. */
	tpoint calculate_best_size_bh(const int width) override;
	void place(const tpoint& origin, const tpoint& size) override;

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from twidget.*/
	bool has_widget(const twidget* widget) const
		{ return twidget::has_widget(widget) || grid_->has_widget(widget); }

	/** Inherited from twidget. */
	void set_origin(const tpoint& origin);

	/** Inherited from twidget. */
	void set_visible_area(const SDL_Rect& area);

	/** Inherited from twidget. */
	void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset);

	void broadcast_frame_buffer(texture& frame_buffer);
	void clear_texture() override;

	void set_margin(int left, int right, int top, int bottom);

protected:
	/** Inherited from twidget. */
	void layout_children() override;

	/** Inherited from twidget. */
	void child_populate_dirty_list(twindow& caller,	const std::vector<twidget*>& call_stack) override;

	bool popup_new_window() override;

public:

	void dirty_under_rect(const SDL_Rect& clip) override
	{
		if (visible_ != VISIBLE) {
			return;
		}
		grid_->dirty_under_rect(clip);
	}

	/** Inherited from tcontrol. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override
	{ 
		if (visible_ != VISIBLE) {
			return nullptr;
		}
		return grid_->find_at(coordinate, must_be_active); 
	}

	/** Inherited from tcontrol.*/
	twidget* find(const std::string& id, const bool must_be_active)
	{
		twidget* result = tcontrol::find(id, must_be_active);
		return result ? result : grid_->find(id, must_be_active);
	}

	/** Inherited from tcontrol.*/
	const twidget* find(const std::string& id, const bool must_be_active) const
	{
		const twidget* result = tcontrol::find(id, must_be_active);
		return result ? result : grid_->find(id, must_be_active);
	}

	/** Inherited from tcontrol. */
	void set_active(const bool active);

	/** Inherited from tcontrol. */
	bool disable_click_dismiss() const;

	/**
	 * Initializes and builds the grid.
	 *
	 * This function should only be called upon an empty grid. This grid is
	 * returned by initial_grid();
	 *
	 * @param grid_builder        The builder for the grid.
	 */
	void init_grid(const boost::intrusive_ptr<tbuilder_grid>& grid_builder);

public:
	/***** ***** ***** setters / getters for members ***** ****** *****/

	// Public due to the fact that window needs to be able to swap the
	// children, might be protected again later.
	const tgrid& grid() const { return *grid_; }
	tgrid& grid() { return *grid_; }

protected:
	void set_container_grid(tgrid& grid) { grid_ = &grid; }

protected:
	tspace4 explicit_margin_;

private:
	/** The grid which holds the child objects. */
	tgrid* grid_;

	/**
	 * Returns the grid to initialize while building.
	 *
	 * @todo Evaluate whether this function is overridden if not remove.
	 */
	virtual tgrid& initial_grid() { return *grid_; }

	virtual void initial_subclass() {}

	/** Returns the space used by the border. */
	tspace4 margin() const;

	virtual tspace4 mini_conf_margin() const { return tspace4{0, 0, 0, 0}; }

	/**
	 * Helper for set_active.
	 *
	 * This function should set the control itself active. It's called by
	 * set_active if the state needs to change. The widget is set to dirty() by
	 * set_active so we only need to change the state.
	 */
	virtual void set_self_active(const bool active) = 0;
};

} // namespace gui2

#endif

