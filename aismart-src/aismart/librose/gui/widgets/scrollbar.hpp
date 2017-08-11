/* $Id: scrollbar.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_SCROLLBAR_HPP_INCLUDED
#define GUI_WIDGETS_SCROLLBAR_HPP_INCLUDED

#include "gui/widgets/control.hpp"

#include <boost/function.hpp>

namespace gui2 {

/**
 * Base class for a scroll bar.
 *
 * class will be subclassed for the horizontal and vertical scroll bar.
 * It might be subclassed for a slider class.
 *
 * To make this class generic we talk a lot about offset and length and use
 * pure virtual functions. The classes implementing us can use the heights or
 * widths, whichever is applicable.
 *
 * The NOTIFY_MODIFIED event is send when the position of scrollbar is changed.
 *
 * Common signal handlers:
 * - connect_signal_notify_modified
 */
class tscrollbar_ : public tcontrol
{
	/** @todo Abstract the code so this friend is no longer needed. */
	friend class tslider;
public:

	explicit tscrollbar_(const bool unrestricted_drag);

	/**
	 * scroll 'step size'.
	 *
	 * When scrolling we always scroll a 'fixed' amount, these are the
	 * parameters for these amounts.
	 */
	enum tscroll {
		BEGIN,               /**< Go to begin position. */
		ITEM_BACKWARDS,      /**< Go one item towards the begin. */
		HALF_JUMP_BACKWARDS, /**< Go half the visible items towards the begin. */
		JUMP_BACKWARDS,      /**< Go the visibile items towards the begin. */
		END,                 /**< Go to the end position. */
		ITEM_FORWARD,        /**< Go one item towards the end. */
		HALF_JUMP_FORWARD,   /**< Go half the visible items towards the end. */
		JUMP_FORWARD };      /**< Go the visible items towards the end. */

	/**
	 * Sets the item position.
	 *
	 * We scroll a predefined step.
	 *
	 * @param scroll              'step size' to scroll.
	 */
	void scroll(const tscroll scroll);

	/** Is the positioner at the beginning of the scrollbar? */
	bool at_begin() const { return item_position_ == 0; }

	/**
	 * Is the positioner at the and of the scrollbar?
	 *
	 * Note both begin and end might be true at the same time.
	 */
	bool at_end() const
		{ return item_position_ + visible_items_ >= item_count_; }

	/** Are all items visible? */
	bool all_items_visible() const { return visible_items_ >= item_count_; }
	bool unrestricted_drag() const { return unrestricted_drag_; }

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void place(const tpoint& origin, const tpoint& size);

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool active)
		{ if(get_active() != active) set_state(active ? ENABLED : DISABLED); };

	/** Inherited from tcontrol. */
	bool get_active() const { return state_ != DISABLED; }

	/** Inherited from tcontrol. */
	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, PRESSED, FOCUSSED, COUNT };
	unsigned get_state() const { return state_; }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void set_item_count(const int item_count)
	{ 
		VALIDATE(item_count >= 0, null_str);
		item_count_ = item_count; 
		recalculate(); 
	}
	int get_item_count() const { return item_count_; }

	/**
	 * Note the position isn't guaranteed to be the wanted position
	 * the step size is honoured. The value will be rouded down.
	 */
	void set_item_position(const int item_position, const bool force_restrict = false);
	int get_item_position() const { return item_position_; }

	int get_visible_items() const { return visible_items_; }
	void set_visible_items(const int visible_items)
	{ 
		VALIDATE(visible_items >= 0, null_str);
		visible_items_ = visible_items; 
		recalculate(); 
	}

	void set_did_modified(const boost::function<void (tscrollbar_& widget)>& did) 
	{
		did_modified_ = did;
	}

protected:
	int get_positioner_offset() const { return positioner_offset_; }

	int get_positioner_length() const { return positioner_length_; }

	/** After a recalculation the canvasses also need to be updated. */
	virtual void update_canvas();

	/**
	 * Callback for subclasses to get notified about positioner movement.
	 *
	 * @todo This is a kind of hack due to the fact there's no simple
	 * callback slot mechanism. See whether we can implement a generic way to
	 * attach callback which would remove quite some extra code.
	 */
	virtual void child_callback_positioner_moved() {}
private:

	void set_state(const tstate state);

private:
	bool unrestricted_drag_;

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/** The number of items the scrollbar 'holds'. */
	int item_count_;

	/** The item the positioner is at, starts maybe negative. */
	int item_position_;

	/**
	 * The number of items which can be shown at the same time.
	 *
	 * As long as all items are visible we don't need to scroll.
	 */
	int visible_items_;

	/**
	 * Number of pixels per step.
	 *
	 * The number of pixels the positioner needs to move to go to the next step.
	 * Note if there is too little space it can happen 1 pixel does more than 1
	 * step.
	 */
	float pixels_per_step_;

	/**
	 * The position the mouse was at the last movement.
	 *
	 * This is used during dragging the positioner.
	 */
	tpoint mouse_;

	/**
	 * The start offset of the positioner.
	 *
	 * This takes the offset before in consideration.
	 */
	int positioner_offset_;

	/** The current length of the positioner. */
	int positioner_length_;

	boost::function<void (tscrollbar_& widget)> did_modified_;

	/***** ***** ***** ***** Pure virtual functions ***** ***** ***** *****/

	/** Get the length of the scrollbar. */
	virtual int get_length() const = 0;

	/** The minimum length of the positioner. */
	virtual int minimum_positioner_length() const = 0;

	/** The maximum length of the positioner. */
	virtual int maximum_positioner_length() const = 0;

	/**
	 * The number of pixels we can't use since they're used for borders.
	 *
	 * These are the pixels before the widget (left side if horizontal,
	 * top side if vertical).
	 */
	virtual int offset_before() const = 0;

	/**
	 * The number of pixels we can't use since they're used for borders.
	 *
	 * These are the pixels after the widget (right side if horizontal,
	 * bottom side if vertical).
	 */
	virtual int offset_after() const = 0;

	/**
	 * Is the coordinate on the positioner?
	 *
	 * @param coordinate          Coordinate to test whether it's on the
	 *                            positioner.
	 *
	 * @returns                   Whether the location on the positioner is.
	 */
	virtual bool on_positioner(const tpoint& coordinate) const = 0;

	/**
	 * Is the coordinate on the bar?
	 *
	 * @param coordinate          Coordinate to test whether it's on the
	 *                            bar.
	 *
	 * @returns                   Whether the location on the bar is.
	 * @retval -1                 Coordinate is on the bar before positioner.
	 * @retval 0                  Coordinate is not on the bar.
	 * @retval 1                  Coordinate is on the bar after the positioner.
	 */
	virtual int on_bar(const tpoint& coordinate) const = 0;

	/**
	 * Gets the relevant difference in between the two positions.
	 *
	 * This function is used to determine how much the positioner needs to  be
	 * moved.
	 */
	virtual int get_length_difference(
		const tpoint& original, const tpoint& current) const = 0;

	/***** ***** ***** ***** Private functions ***** ***** ***** *****/

	/**
	 * Updates the scrollbar.
	 *
	 * Needs to be called when someting changes eg number of items
	 * or available size. It can only be called once we have a size
	 * otherwise we can't calulate a thing.
	 */
	void recalculate();

	/**
	 * Updates the positioner.
	 *
	 * This is a helper for recalculate().
	 */
	void recalculate_positioner();

	/**
	 * Moves the positioner.
	 *
	 * @param distance           The distance moved, negative to begin, positive
	 *                           to end.
	*/
	void move_positioner(const int distance);

	/** Inherited from tcontrol. */
	void load_config_extra() override;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_mouse_enter(
			const event::tevent event, bool& handled, bool& halt);

	void signal_handler_mouse_motion(
			  const event::tevent event
			, bool& handled
			, bool& halt
			, const tpoint& coordinate);

	void signal_handler_mouse_leave(const event::tevent event, bool& handled);

	void signal_handler_left_button_down(
			const event::tevent event, bool& handled);

	void signal_handler_left_button_up(
			const event::tevent event, bool& handled);

};

} // namespace gui2

#endif

