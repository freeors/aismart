/* $Id: scroll_container.hpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#ifndef GUI_WIDGETS_SCROLL_CONTAINER_HPP_INCLUDED
#define GUI_WIDGETS_SCROLL_CONTAINER_HPP_INCLUDED

#include "gui/widgets/container.hpp"
#include "gui/widgets/scrollbar.hpp"
#include "gui/widgets/timer.hpp"

namespace gui2 {

class tspacer;
class ttoggle_panel;
class ttrack;

namespace implementation {
	struct tbuilder_scroll_text_box;
	struct tbuilder_report;
	struct tbuilder_scroll_panel;
}

#define GC_PRECISE_DISTANCE_THRESHOLD	500000000
#define gc_join_estimated(value)		((value) + GC_PRECISE_DISTANCE_THRESHOLD)
#define gc_split_estimated(value)		((value) - GC_PRECISE_DISTANCE_THRESHOLD)
#define gc_is_estimated(value)			((value) != nposm && (value) >= GC_PRECISE_DISTANCE_THRESHOLD)
#define gc_is_precise(value)			((value) != nposm && (value) < GC_PRECISE_DISTANCE_THRESHOLD)
#define gc_distance_value(value)		(gc_is_estimated(value)? gc_split_estimated(value): value)

/**
 * Base class for creating containers with one or two scrollbar(s).
 *
 * For now users can't instanciate this class directly and needs to use small
 * wrapper classes. Maybe in the future users can use the class directly.
 *
 * @todo events are not yet send to the content grid.
 */
class tscroll_container: public tcontainer_
{
	friend struct implementation::tbuilder_scroll_text_box;
	friend struct implementation::tbuilder_report;
	friend struct implementation::tbuilder_scroll_panel;
	friend class tlistbox;
	friend class ttree;
	friend class tscroll_text_box;
	friend class treport;
	friend class tscroll_panel;

public:
	static bool refuse_scroll;
	struct trefuse_scroll_lock
	{
		trefuse_scroll_lock()
		{
			VALIDATE(!refuse_scroll, null_str);
			refuse_scroll = true;
		}

		~trefuse_scroll_lock()
		{
			refuse_scroll = false;
		}
	};

	explicit tscroll_container(const unsigned canvas_count, bool listbox = false);

	~tscroll_container();

	/** The way to handle the showing or hiding of the scrollbar. */
	enum tscrollbar_mode {
		always_invisible,         /**<
		                           * The scrollbar is never shown even not
		                           * when needed. There's also no space
		                           * reserved for the scrollbar.
		                           */
		auto_visible,             /**<
		                           * The scrollbar is shown when the number of
		                           * items is larger as the visible items. The
		                           * space for the scrollbar is always
		                           * reserved, just in case it's needed after
		                           * the initial sizing (due to adding items).
		                           */
	};

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/
	const tgrid& layout_grid() const { return *content_grid_; }

	void reset_scrollbar(twindow& window);

	/***** ***** ***** scrollbar helpers ***** ****** *****/

	/**
	 * Scrolls the vertical scrollbar.
	 *
	 * @param scroll              The position to scroll to.
	 */
	void scroll_vertical_scrollbar(const tscrollbar_::tscroll scroll);

	/**
	 * Callback when the scrollbar moves (NOTE maybe only one callback needed).
	 * Maybe also make protected or private and add a friend.
	 */
	void vertical_scrollbar_moved();

	void horizontal_scrollbar_moved();

	void set_scroll_to_end(bool val) { scroll_to_end_ = val; }

	void invalidate_layout(twidget* widget) override;

	void set_find_content_grid(bool val) { find_content_grid_ = val; }

	tgrid* content_grid() { return content_grid_; }
	const tgrid* content_grid() const { return content_grid_; }

	/** Inherited from tcontainer_. */
	twidget* find(const std::string& id, const bool must_be_active);

	/** Inherited from tcontrol.*/
	const twidget* find(const std::string& id, const bool must_be_active) const;

	int pullrefresh_get_default_drag_height(int* top, int* middle, int* bottom) const;
	void set_did_pullrefresh(const int refresh_height, const boost::function<void (ttrack&, const SDL_Rect&)>& did_refresh,
		const int min_drag2_height = nposm, const boost::function<void (ttrack&, const SDL_Rect&)>& did_drag1 = NULL, const boost::function<void (ttrack&, const SDL_Rect&)>& did_drag2 = NULL);

	// called by tdialog.
	void pullrefresh_refresh(const SDL_Rect& area);

protected:
	/** Inherited from tcontainer_. */
	void layout_init(bool linked_group_only) override;

	SDL_Rect get_float_widget_ref_rect() const override;

	/** Inherited from tcontainer_. */
	tpoint calculate_best_size() const override;
	tpoint calculate_best_size_bh(const int width) override;
	tpoint mini_get_best_text_size() const override;

	/** Inherited from tcontainer_. */
	void place(const tpoint& origin, const tpoint& size);

	/** Inherited from tcontainer_. */
	void set_origin(const tpoint& origin);

	/** Inherited from tcontainer_. */
	void set_visible_area(const SDL_Rect& area);

	/***** ***** ***** inherited ****** *****/

	/** Inherited from tcontainer_. */
	bool get_active() const { return state_ != DISABLED; }

	/** Inherited from tcontainer_. */
	unsigned get_state() const { return state_; }

	void dirty_under_rect(const SDL_Rect& clip) override;

	/** Inherited from tcontainer_. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active) override;

	/** Inherited from tcontainer_. */
	bool disable_click_dismiss() const;

	/***** ***** ***** setters / getters for members ***** ****** *****/

	/** @note shouldn't be called after being shown in a dialog. */
	void set_vertical_scrollbar_mode(const tscrollbar_mode scrollbar_mode);
	tscrollbar_mode get_vertical_scrollbar_mode() const
		{ return vertical_scrollbar_mode_; }

	/** @note shouldn't be called after being shown in a dialog. */
	void set_horizontal_scrollbar_mode(const tscrollbar_mode scrollbar_mode);
	tscrollbar_mode get_horizontal_scrollbar_mode() const
		{ return horizontal_scrollbar_mode_; }

	const tscrollbar_* vertical_scrollbar() const { return vertical_scrollbar_; }

	const tspacer* content_spacer() const { return content_; }

	const SDL_Rect& content_visible_area() const { return content_visible_area_; }

	/** The builder needs to call us so we do our setup. */
	void finalize_setup(const tbuilder_grid_const_ptr& grid_builder); // FIXME make protected

private:
	virtual void mini_more_items() {}

	virtual tpoint mini_get_best_content_grid_size() const { return tpoint(0, 0); }
	virtual void adjust_offset(int& x_offset, int& y_offset) {}
	virtual void mini_set_content_grid_origin(const tpoint& origin, const tpoint& content_origin);
	virtual void mini_set_content_grid_visible_area(const SDL_Rect& area);
	virtual void mini_mouse_down(const tpoint& first) {}
	virtual bool mini_mouse_motion(const tpoint& first, const tpoint& last) { return true; }
	virtual void mini_mouse_leave(const tpoint& first, const tpoint& last) {}
	virtual bool mini_wheel() { return true; }

	// it is time to Garbage Collection
	int gc_calculate_total_height() const;
	int gc_which_precise_row(const int distance) const;
	int gc_which_children_row(const int distance) const;

	virtual int mini_handle_gc(const int x_offset, const int y_offset);
	virtual int gc_handle_rows() const { return 0; }
	virtual tpoint gc_handle_calculate_size(ttoggle_panel& widget, const int width) const;
	virtual ttoggle_panel* gc_widget_from_children(const int at) const { return nullptr; }
	virtual void gc_handle_garbage_collection(ttoggle_panel& widget);
	virtual void layout_init2() {};
	virtual void show_row_rect(const int at) {};
	virtual int gc_handle_update_height(const int new_height) { return 0; }
	virtual void validate_children_continuous() const {};

	bool pullrefresh_enabled() const { return pullrefresh_min_drag2_height_ != nposm; };
	void did_default_pullrefresh_drag1(ttrack& widget, const SDL_Rect& rect);
	void did_default_pullrefresh_drag2(ttrack& widget, const SDL_Rect& rect);

protected:
	/**
	 * Shows a certain part of the content.
	 *
	 * When the part to be shown is bigger as the visible viewport the top
	 * left of the wanted rect will be the top left of the viewport.
	 *
	 * @param rect                The rect which should be visible.
	 */
	void show_content_rect(const SDL_Rect& rect);

	/*
	 * The widget contains the following three grids.
	 *
	 * * _vertical_scrollbar_grid containing at least a widget named
	 *   _vertical_scrollbar
	 *
	 * * _horizontal_scrollbar_grid containing at least a widget named
	 *   _horizontal_scrollbar
	 *
	 * * _content_grid a grid which holds the contents of the area.
	 *
	 * NOTE maybe just hardcode these in the finalize phase...
	 *
	 */

	void vertical_set_item_position(const int item_position);
	void horizontal_set_item_position(const int item_position);

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/**
	 * Home key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_home(SDL_Keymod modifier, bool& handled);

	/**
	 * End key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_end(SDL_Keymod modifier, bool& handled);

	/**
	 * Page up key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_page_up(SDL_Keymod modifier, bool& handled);

	/**
	 * Page down key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_page_down(SDL_Keymod modifier, bool& handled);


	/**
	 * Up arrow key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_up_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Down arrow key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_down_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Left arrow key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_left_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Right arrow key pressed.
	 *
	 * @param modifier            The SDL keyboard modifier when the key was
	 *                            pressed.
	 * @param handled             If the function handles the key it should
	 *                            set handled to true else do not modify it.
	 *                            This is used in the keyboard event
	 *                            changing.
	 */
	virtual void handle_key_right_arrow(SDL_Keymod modifier, bool& handled);

	void set_scrollbar_mode(tscrollbar_& scrollbar,
		const tscrollbar_mode& scrollbar_mode,
		const unsigned items, const unsigned visible_items);

	void set_first_coordinate_null(const tpoint& last_coordinate);
	int can_vertical_wheel(const int diff_y, const bool up) const;

private:
	bool is_scroll_container() const override { return true; }

	void construct_spacer_grid(tgrid& grid);

	virtual tgrid* mini_create_content_grid();
	virtual void mini_finalize_subclass() {}

	/** Inherited from tcontainer_. */
	void layout_children();

	/** Inherited from tcontainer_. */
	void impl_draw_children(texture& frame_buffer, int x_offset, int y_offset);

	void broadcast_frame_buffer(texture& frame_buffer);
	void clear_texture() override;

	/** Inherited from tcontainer_. */
	void child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack) override;

	bool popup_new_window() override;

	virtual tpoint mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size) = 0;

	void scroll_timer_handler(const bool vertical, const bool up, const int level);
	bool scroll(const bool vertical, const bool up, int level, const bool first);

	/** Helper function which needs to be called after the scollbar moved. */
	void scrollbar_moved(bool gc_handled = false);

protected:
	bool listbox_;
	bool scroll_to_end_;
	bool unrestricted_drag_;

	/** The grid that holds the content. */
	tgrid* content_grid_;

	/** Dummy spacer to hold the contents location. */
	tspacer* content_;

	bool need_layout_;

	/**
	 * The mode of how to show the scrollbar.
	 *
	 * This value should only be modified before showing, doing it while
	 * showing results in UB.
	 */
	tscrollbar_mode	vertical_scrollbar_mode_, horizontal_scrollbar_mode_;

	/** These are valid after finalize_setup(). */
	tscrollbar_* vertical_scrollbar_;
	tscrollbar_* horizontal_scrollbar_;

	tscrollbar_* dummy_vertical_scrollbar_;
	tscrollbar_* dummy_horizontal_scrollbar_;

	tfloat_widget* vertical_scrollbar2_;
	tfloat_widget* horizontal_scrollbar2_;

	ttimer scroll_timer_;
	std::pair<int, int> scroll_elapse_;
	uint32_t next_scrollbar_invisible_ticks_;

	tpoint first_coordinate_;
	bool can_scroll_;
	bool outermost_widget_;

	bool find_content_grid_;

	int gc_next_precise_at_;
	int gc_first_at_;
	int gc_last_at_;
	int gc_current_content_width_;
	int gc_locked_at_;

	struct tgc_calculate_best_size_lock {
		tgc_calculate_best_size_lock(tscroll_container& scroll_container)
			: scroll_container(scroll_container)
		{
			VALIDATE(!scroll_container.gc_calculate_best_size_, null_str);
			scroll_container.gc_calculate_best_size_ = true;
		}
		~tgc_calculate_best_size_lock()
		{
			scroll_container.gc_calculate_best_size_ = false;
		}
		tscroll_container& scroll_container;
	};
	bool gc_calculate_best_size_;

	std::unique_ptr<tout_of_chain_widget_lock> out_of_chain_widget_lock_;
	bool adaptive_visible_scrollbar_;

private:
	tgrid grid_;
	tspacer* vertical_drag_spacer_;
	ttrack* pullrefresh_track_;

	enum {pullrefresh_drag1, pullrefresh_drag2, pullrefresh_springback};
	int pullrefresh_state_;
	int pullrefresh_min_drag2_height_;
	int pullrefresh_refresh_height_;
	boost::function<void (ttrack&, const SDL_Rect&)> did_pullrefresh_drag1_;
	boost::function<void (ttrack&, const SDL_Rect&)> did_pullrefresh_drag2_;
	boost::function<void (ttrack&, const SDL_Rect&)> did_pullrefresh_refresh_;

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	enum tstate { ENABLED, DISABLED, COUNT };

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/**
	 * Cache for the visible area for the content.
	 *
	 * The visible area for the content needs to be updated when scrolling.
	 */
	SDL_Rect content_visible_area_;

	bool calculate_reduce_;


	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_sdl_key_down(const event::tevent event
			, bool& handled
			, const SDL_Keycode key
			, SDL_Keymod modifier);

	void signal_handler_sdl_wheel_up(bool& handled, const tpoint& coordinate2);
	void signal_handler_sdl_wheel_down(bool& handled, const tpoint& coordinate);
	void signal_handler_sdl_wheel_left(bool& handled, const tpoint& coordinate);
	void signal_handler_sdl_wheel_right(bool& handled, const tpoint& coordinate);

	void signal_handler_mouse_enter(const tpoint& coordinate, bool pre_child);
	void signal_handler_left_button_down(const tpoint& coordinate, bool pre_child);
	void signal_handler_left_button_up(const tpoint& coordinate, bool pre_child);
	void signal_handler_mouse_leave(const tpoint& coordinate, const tpoint& coordinate2, bool pre_child);
	void signal_handler_mouse_motion(bool& handled, bool& halt, const tpoint& coordinate, const tpoint& coordinate2, bool pre_child);

	virtual void signal_handler_out_of_chain(const int e, twidget* extra_widget) {}
};

} // namespace gui2

#endif

