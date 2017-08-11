/* $Id: window.cpp 54906 2012-07-29 19:52:01Z mordante $ */
/*
   Copyright (C) 2007 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Implementation of window.hpp.
 */

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/window.hpp"
#include "gui/dialogs/dialog.hpp"
#include "font.hpp"
#include "area_anim.hpp"
#include "gettext.hpp"
#include "gui/auxiliary/event/distributor.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_panel.hpp"
#include <preferences.hpp>
#include "preferences_display.hpp"
#include "video.hpp"
#include "formula_string_utils.hpp"
#include "hotkeys.hpp"

#include <boost/bind.hpp>

namespace gui2{

const int ttransition::normal_duration = 200; // 2 second
// const int ttransition::normal_duration = 5000; // 2 second

void ttransition::set_transition(surface& surf, int start, int duration)
{ 
	transition_surf_ = clone_surface(surf);
	transition_start_time_ = start; 
	transition_duration_ = duration;
}

namespace implementation {
/** @todo See whether this hack can be removed. */
// Needed to fix a compiler error in REGISTER_WIDGET.
class tbuilder_window
	: public tbuilder_control
{
public:
	tbuilder_window(const config& cfg)
		: tbuilder_control(cfg)
	{
	}

	twidget* build() const { return NULL; }
};

} // namespace implementation
REGISTER_WIDGET(window)

bool twindow::eat_left_up = false;
const std::string twindow::visual_layout_id = "rose__visual_layout";
surface twindow::last_frame_buffer;
twindow* twindow::init_instance = nullptr;

namespace {
/**
 * The interval between draw events.
 *
 * When the window is shown this value is set, the callback function always
 * uses this value instead of the parameter send, that way the window can stop
 * drawing when it wants.
 */
static int draw_interval = 0;

/**
 * SDL_AddTimer() callback for delay_event.
 *
 * @param event                   The event to push in the event queue.
 *
 * @return                        The new timer interval (always 0).
 */
static Uint32 delay_event_callback(const Uint32, void* event)
{
	SDL_PushEvent(static_cast<SDL_Event*>(event));
	return 0;
}

/**
 * Allows an event to be delayed a certain amount of time.
 *
 * @note the delay is the minimum time, after the time has passed the event
 * will be pushed in the SDL event queue, so it might delay more.
 *
 * @param event                   The event to delay.
 * @param delay                   The number of ms to delay the event.
 */
static void delay_event(const SDL_Event& event, const Uint32 delay)
{
	SDL_AddTimer(delay, delay_event_callback, new SDL_Event(event));
}

} // namespace

bool twindow::set_orientation_resolution()
{
	SDL_Rect rect = screen_area();

	if (!should_conside_orientation(rect.w, rect.h)) {
		return false;
	}
	int swapped_width = rect.h;
	int swapped_height = rect.w;
	preferences::set_resolution(anim2::manager::instance->video(), swapped_width, swapped_height, false);
	// update_screen_size();

		rect = screen_area();
		settings::screen_width = rect.w;
		settings::screen_height = rect.h;

	return true;
}

void twindow::enter_orientation(torientation orientation)
{
	if (!should_conside_orientation(settings::screen_width, settings::screen_height)) {
		return;
	}

	bool original_landscape = current_landscape;
	current_landscape = landscape_from_orientation(orientation, current_landscape);
	if (current_landscape != original_landscape) {
		set_orientation_resolution();
	}
}

void twindow::recover_landscape(bool original_landscape)
{
	if (!should_conside_orientation(settings::screen_width, settings::screen_height)) {
		return;
	}

	if (current_landscape != original_landscape) {
		current_landscape = original_landscape;
		set_orientation_resolution();
	}
}

twindow::twindow(CVideo& video,
		tformula<unsigned>x,
		tformula<unsigned>y,
		tformula<unsigned>w,
		tformula<unsigned>h,
		const bool automatic_placement,
		const unsigned horizontal_placement,
		const unsigned vertical_placement,
		const std::string& definition,
		const bool scene,
		const torientation orientation,
		const unsigned explicit_x,
		const unsigned explicit_y)
	: tpanel()
	, cursor::setter(cursor::NORMAL)
	, video_(video)
	, status_(NEW)
	, retval_(NONE)
	, owner_(0)
	, need_layout_(true)
	, variables_()
	, suspend_drawing_(true)
	, restorer_()
	, automatic_placement_(automatic_placement)
	, horizontal_placement_(horizontal_placement)
	, vertical_placement_(vertical_placement)
	, x_(x)
	, y_(y)
	, w_(w)
	, h_(h)
	, explicit_x_(explicit_x)
	, explicit_y_(explicit_y)
	, layouted_(false)
	, drawn_(false)
	, click_dismiss_(false)
	, leave_dismiss_(false)
	, enter_disabled_(false)
	, escape_disabled_(false)
	, linked_size_()
	, dirty_list_()
	, scene_(scene)
	, orientation_(orientation)
	, original_landscape_(current_landscape)
	, tooltip_at_(nposm)
	, drag_widget_(nullptr)
	, event_distributor_(new event::tdistributor(
			*this, event::tdispatcher::front_child))
{
	can_invalidate_layout_ = true;

	enter_orientation(orientation_);
	
	// We load the config in here as exception.
	// Our caller did update the screen size so no need for us to do that again.
	set_definition(definition, null_str);

	event::connect_dispatcher(this);

	connect_signal<event::SDL_VIDEO_RESIZE>(
			  boost::bind(&twindow::signal_handler_sdl_video_resize
				  , this, _2, _3, _5));

	connect_signal<event::MOUSE_MOTION>(boost::bind(
				&twindow::signal_handler_mouse_motion, this, _5), event::tdispatcher::back_post_child);

	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&twindow::signal_handler_mouse_leave, this, _5, _6), event::tdispatcher::back_post_child);

	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				  &twindow::signal_handler_click_dismiss, this, _2, _3, _4)
			, event::tdispatcher::back_post_child);

	connect_signal<event::SDL_KEY_DOWN>(
			  boost::bind(&twindow::signal_handler_sdl_key_down
				  , this, _2, _3, _5)
			, event::tdispatcher::back_pre_child);
	connect_signal<event::SDL_KEY_DOWN>(
			  boost::bind(&twindow::signal_handler_sdl_key_down
				  , this, _2, _3, _5));

	connect_signal<event::MESSAGE_SHOW_TOOLTIP>(
			  boost::bind(
				  &twindow::signal_handler_message_show_tooltip
				, this
				, _2
				, _3
				, _5)
			, event::tdispatcher::back_pre_child);
}

twindow::~twindow()
{
	/*
	 * We need to delete our children here instead of waiting for the grid to
	 * automatically do it. The reason is when the grid deletes its children
	 * they will try to unregister them self from the linked widget list. At
	 * this point the member of twindow are destroyed and we enter UB. (For
	 * some reason the bug didn't trigger on g++ but it does on MSVC.
	 */
	for (unsigned row = 0; row < grid().get_rows(); ++row) {
		for(unsigned col = 0; col < grid().get_cols(); ++col) {
			grid().remove_child(row, col);
		}
	}

	/*
	 * The tip needs to be closed if the window closes and the window is
	 * not a tip. If we don't do that the tip will unrender in the next
	 * window and cause drawing glitches.
	 * Another issue is that on smallgui and an MP game the tooltip not
	 * unrendered properly can capture the mouse and make playing impossible.
	 */
	remove_tooltip();

	delete event_distributor_;

	recover_landscape(original_landscape_);

	event::disconnect_dispatcher(this);

	remove_timer_during_destructor();
}

void twindow::set_retval(const int retval) 
{ 
	VALIDATE(!is_closing(), null_str);

	retval_ = retval;
	close();
}

void twindow::window_connect_disconnected(const bool connect)
{
	event_distributor_->window_connect_disconnected(connect);
}

void twindow::update_screen_size()
{
	// Only if we're the toplevel window we need to update the size, otherwise
	// it's done in the resize event.
	if (draw_interval == 0) {
		const SDL_Rect rect = screen_area();
		settings::screen_width = rect.w;
		settings::screen_height = rect.h;
	}
}

twindow::tretval twindow::get_retval_by_id(const std::string& id)
{
/*WIKI
 * @page = GUIToolkitWML
 * @order = 3_widget_window_2
 *
 * List if the id's that have generate a return value:
 * * ok confirms the dialog.
 * * cancel cancels the dialog.
 *
 */
	// Note it might change to a map later depending on the number
	// of items.
	if(id == "ok") {
		return OK;
	} else if(id == "cancel") {
		return CANCEL;

	// default if nothing matched
	} else {
		return NONE;
	}
}

void twindow::show_slice()
{
	events::pump();
	absolute_draw();
	// Add a delay so we don't keep spinning if there's no event.
	SDL_Delay(10);
}

int twindow::show(const bool restore)
{
	/**
	 * Helper class to set and restore the drawing interval.
	 *
	 * We need to make sure we restore the value when the function ends, be it
	 * normally or due to an exception.
	 */
	class tdraw_interval_setter
	{
	public:
		tdraw_interval_setter()
			: interval_(draw_interval)
		{
			if (interval_ == 0) {
				draw_interval = 30;

				// There might be some time between creation and showing so
				// reupdate the sizes.
				update_screen_size();

			}
		}

		~tdraw_interval_setter()
		{
			draw_interval = interval_;
		}
	private:

		int interval_;
	};

	tdraw_interval_setter draw_interval_setter;

	/*
	 * Before show has been called, some functions might have done some testing
	 * on the window and called layout, which can give glitches. So
	 * reinvalidate the window to avoid those glitches.
	 */
	invalidate_layout(nullptr);
	suspend_drawing_ = false;

	try {
		// Start our loop drawing will happen here as well.
		for (status_ = (status_ == REQUEST_CLOSE)? status_: SHOWING; status_ != REQUEST_CLOSE; ) {
			// process installed callback if valid, to allow e.g. network polling
			show_slice();
		}
	} catch (CVideo::quit&) {
		// on window, press right-top's close button. 
		// it is terminate app normally, make sure tdialog::post_show be called.
		retval_ = APP_TERMINATE;

	} catch (...) {
		// other exception handled by caller.
		throw;

		/**
		 * @todo Clean up the code duplication.
		 *
		 * In the future the restoring shouldn't be needed so the duplication
		 * doesn't hurt too much but keep this todo as a reminder.
		 */

	}

	suspend_drawing_ = true;

	// restore area
	if (restore) {
		SDL_Rect rect = get_rect();
		render_surface(get_renderer(), restorer_, NULL, &rect);
		font::undraw_floating_labels();
	}

	return retval_;
}

extern SDL_Rect dbg_start_rect;
extern SDL_Rect dbg_end_rect;

void twindow::draw()
{
	/***** ***** ***** ***** Init ***** ***** ***** *****/
	// Prohibited from drawing?
	if (suspend_drawing_) {
		return;
	}

	// texture frame_buffer = video_.getTexture();
	texture frame_buffer = get_screen_texture();
	int frame_buffer_width, frame_buffer_height;
	SDL_QueryTexture(frame_buffer.get(), NULL, NULL, &frame_buffer_width, &frame_buffer_height);

	/***** ***** Layout and get dirty list ***** *****/
	if (need_layout_) {
		VALIDATE(!scene_, "layout must not be false during draw.");

		// Restore old surface. In the future this phase will not be needed
		// since all will be redrawn when needed with dirty rects. Since that
		// doesn't work yet we need to undraw the window.
		// new rect maybe less than old's.
		if (restorer_) {
			SDL_Rect rect = get_rect();
			render_surface(get_renderer(), restorer_, NULL, &rect);
			// Since the old area might be bigger as the new one, invalidate it.
		}

		layout();

		// Get new surface for restoring
		SDL_Rect rect = get_rect();
		// We want the labels underneath the window so draw them and use them
		// as restore point.
		font::draw_floating_labels();
		restorer_ = get_surface_portion(frame_buffer, rect);

		// Need full redraw so only set ourselves dirty.
		add_to_dirty_list(std::vector<twidget*>(1, this));
	} else {

		// Let widgets update themselves, which might dirty some things.
		layout_children();

		// Now find the widgets that are dirty.
		std::vector<twidget*> call_stack;

		populate_dirty_list(*this, call_stack);
	}

	Uint32 now = SDL_GetTicks();
	bool require_clone = false;
	const SDL_Rect window_rect = get_rect();

/*
	if (transition_start_time_ != nposm) {
		if (transition_surf_ && (int)now < transition_start_time_ + transition_duration_) {
			if (window_rect.w == transition_surf_->w && window_rect.h == transition_surf_->h) {
				require_clone = true;

			} else if (window_rect.y + window_rect.h == transition_surf_->h) {
				if (!transition_frame_buffer_) {
					adjust_surface_color2(frame_buffer, -50, -50, -50);
				}
				require_clone = true;
			}

			if (require_clone) {
				if (!transition_frame_buffer_) {
					transition_frame_buffer_ = SDL_CreateRGBSurface(0, frame_buffer->w, frame_buffer->h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
					SDL_SetSurfaceBlendMode(transition_frame_buffer_, SDL_BLENDMODE_NONE);
				}
				frame_buffer = transition_frame_buffer_;
			}

		}
	}
*/
	int xsrc = 0, ysrc = 0;

	BOOST_FOREACH(std::vector<twidget*>& item, dirty_list_) {

		twidget* terminal = item.back();

		const SDL_Rect dirty_rect = terminal->get_dirty_rect();

		for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it) {
			tfloat_widget& item = *(it->get());
			tcontrol& widget = *(item.widget.get());
			if (widget.get_visible() != twidget::VISIBLE) {
				continue;
			}
			SDL_Rect rect = widget.get_rect();
			if (SDL_RectEmpty(&rect)) {
				continue;
			}
			std::vector<SDL_Rect>& rects = item.buf_rects;
			if (rects.size() == 1 && rects[0] == rect) {
				continue;
			}
			SDL_Rect result;
			if (SDL_IntersectRect(&rect, &dirty_rect, &result)) {
				rects.push_back(result);
			}
			
		}

		texture_clip_rect_setter clip(&dirty_rect);
		/*
		 * The actual update routine does the following:
		 * - Restore the background.
		 *
		 * - draw [begin, end) the back ground of all widgets.
		 *
		 * - draw the children of the last item in the list, if this item is
		 *   a container it's children get a full redraw. If it's not a
		 *   container nothing happens.
		 *
		 * - draw [rbegin, rend) the fore ground of all widgets. For items
		 *   which have two layers eg window or panel it draws the foreground
		 *   layer. For other widgets it's a nop.
		 *
		 * Before drawing there needs to be determined whether a dirty widget
		 * really needs to be redrawn. If the widget doesn't need to be
		 * redrawing either being not VISIBLE or has status NOT_DRAWN. If
		 * it's not drawn it's still set not dirty to avoid it keep getting
		 * on the dirty list.
		 */

		for (std::vector<twidget*>::iterator itor = item.begin(); itor != item.end(); ++itor) {
			if ((**itor).get_visible() != twidget::VISIBLE || (**itor).get_drawing_action() == twidget::NOT_DRAWN) {

				for (std::vector<twidget*>::iterator citor = itor; citor != item.end(); ++citor) {

					(**citor).clear_dirty();
				}

				item.erase(itor, item.end());
				break;
			}
		}

		if (!is_scene()) {
			// Restore.
			render_surface(get_renderer(), restorer_, NULL, &window_rect);
		}

		/**
		 * @todo Remove the if an always use the true branch.
		 *
		 * When removing that code the draw functions with only a frame_buffer
		 * as parameter should also be removed since they will be unused from
		 * that moment.
		 */

		// Background.
		for (std::vector<twidget*>::iterator itor = item.begin(); itor != item.end(); ++ itor) {
			twidget* widget = *itor;

			widget->draw_background(frame_buffer, xsrc, ysrc);

			if (widget == terminal) {
				widget->draw_children(frame_buffer, xsrc, ysrc);
			}
		}

		// Foreground.
		for (std::vector<twidget*>::reverse_iterator ritor = item.rbegin(); ritor != item.rend(); ++ritor) {
			twidget& widget = **ritor;
			widget.clear_dirty();
		}
	}

	dirty_list_.clear();

	SDL_Rect src_r = ::create_rect(0, 0, frame_buffer_width, frame_buffer_height);
	SDL_Rect dst_r = src_r;
	bool restore_from_transition_surf = false;

	if (!SDL_RectEmpty(&dbg_start_rect)) {
		render_rect(get_renderer(), dbg_start_rect, 0x10ff0000);
	}
	if (!SDL_RectEmpty(&dbg_end_rect)) {
		render_rect(get_renderer(), dbg_end_rect, 0x1000ff00);
	}
/*
	if (transition_start_time_ != nposm) {
		if (transition_surf_ && (int)now < transition_start_time_ + transition_duration_) {
			surface frame_buffer2 = video_.getSurface();

			if (window_rect.w == transition_surf_->w && window_rect.h == transition_surf_->h) {
				VALIDATE(require_clone, null_str);

				const int threshold = window_rect.w / 4;
				int remain = transition_start_time_ + transition_duration_ - now;
				// why divided by 2? minimum transite offset is 1/2 windowrect.w.
				xsrc = (window_rect.w / 2) * remain / transition_duration_;

				if (transition_fade_ == fade_left || transition_fade_ == fade_right) {
					if (xsrc < threshold) {
						// reatch threshold, blit transition surface.
						if (transition_fade_ == fade_left) {
							src_r.x = threshold - xsrc;
							src_r.w = window_rect.w - src_r.x;

							dst_r.x = 0;
						} else {
							src_r.x = 0;
							src_r.w = window_rect.w - (threshold - xsrc);

							dst_r.x = threshold - xsrc;
						}
						dst_r.y = 0;

						sdl_blit(transition_surf_, &src_r, frame_buffer2, &dst_r);
					}

					if (transition_fade_ == fade_left) {
						src_r.x = 0;
						dst_r.x = xsrc;

					} else {
						src_r.x = xsrc;
						dst_r.x = 0;
					}
					src_r.w = window_rect.w - xsrc;

					sdl_blit(transition_frame_buffer_, &src_r, frame_buffer2, &dst_r);

				} else {
					restore_from_transition_surf = true;
				}

			} else if (window_rect.y + window_rect.h == transition_surf_->h) {
				int remain = transition_start_time_ + transition_duration_ - now;
				ysrc = (window_rect.h / 2 ) * remain / transition_duration_;

				// don't blit transition surface.
				src_r = ::create_rect(window_rect.x, window_rect.y, window_rect.w, window_rect.h - ysrc);
				dst_r.x = window_rect.x;
				dst_r.y = window_rect.y + ysrc;
				sdl_blit(transition_frame_buffer_, &src_r, frame_buffer2, &dst_r);

			} else {
				restore_from_transition_surf = true;
			}

		} else {
			restore_from_transition_surf = true;
		}
	}

	if (restore_from_transition_surf) {
		if (transition_frame_buffer_) {
			dst_r.x = window_rect.x;
			dst_r.y = window_rect.y;
			surface frame_buffer2 = video_.getSurface();
			sdl_blit(transition_frame_buffer_, &window_rect, frame_buffer2, &dst_r);
			transition_frame_buffer_ = NULL;

			broadcast_frame_buffer(frame_buffer2);
		}
		transition_start_time_ = nposm;
	}
*/

/*
	std::vector<twidget*> call_stack;
	populate_dirty_list(*this, call_stack);
	VALIDATE(dirty_list_.empty(), "twinodw::draw, has dirty control!");
*/
}

std::vector<std::vector<twidget*> >& twindow::dirty_list()
{
	return dirty_list_;
}

void twindow::undraw()
{
	if(restorer_) {
		SDL_Rect rect = get_rect();
		render_surface(get_renderer(), restorer_, NULL, &rect);
		// Since the old area might be bigger as the new one, invalidate
		// it.
	}
}

void twindow::invalidate_layout(twidget* widget)
{
	if (!scene_ && !disalbe_invalidate_layout) {
		need_layout_ = true;
	}
}

void twindow::click_edit_button(const int id)
{
	if (did_edit_click_) {
		did_edit_click_(*this, id);
	}
}

void twindow::set_did_edit_click(const boost::function<void (twindow&, const int)>& did)
{
	did_edit_click_ = did;
}

void twindow::dirty_under_rect(const SDL_Rect& clip)
{
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_reverse_iterator it = float_widgets_.rbegin(); it != float_widgets_.rend(); ++it) {
		const tfloat_widget& item = **it;
		tcontrol& widget = *item.widget;
		if (widget.get_visible() != twidget::VISIBLE || (item.ref_.empty() && !item.ref_widget_)) {
			continue;
		}
		widget.dirty_under_rect(clip);
	}
	tcontainer_::dirty_under_rect(clip);
}

twidget* twindow::float_widget_find_at(const tpoint& coordinate, const bool must_be_active) const
{
	twidget* result = nullptr;
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_reverse_iterator it = float_widgets_.rbegin(); it != float_widgets_.rend(); ++it) {
		const tfloat_widget& item = **it;
		if (game_config::mobile && item.is_scrollbar) {
			// on mobile, scrollbar is used for indicate only.
			continue;
		}
		tcontrol& widget = *item.widget;
		if (widget.get_visible() != twidget::VISIBLE || (item.ref_.empty() && !item.ref_widget_)) {
			continue;
		}
		result = widget.find_at(coordinate, must_be_active);
		if (result) {
			return result;
		}
	}
	return result;
}

twidget* twindow::float_widget_find(const std::string& id, const bool must_be_active) const
{
	twidget* result = nullptr;
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it) {
		tcontrol& widget = *(*it)->widget;
		result = widget.find(id, must_be_active);
		if (result) {
			return result;
		}
	}
	return result;
}

twidget* twindow::find_at(const tpoint& coordinate, const bool must_be_active)
{ 
	twidget* result = float_widget_find_at(coordinate, must_be_active);
	if (result) {
		return result;
	}
	return tpanel::find_at(coordinate, must_be_active); 
}

twidget* twindow::find(const std::string& id, const bool must_be_active)
{ 
	twidget* result = float_widget_find(id, must_be_active);
	if (result) {
		return result;
	}
	return tcontainer_::find(id, must_be_active); 
}

const twidget* twindow::find(const std::string& id, const bool must_be_active) const
{ 
	const twidget* result = float_widget_find(id, must_be_active);
	if (result) {
		return result;
	}
	return tcontainer_::find(id, must_be_active); 
}

// @ true: in widget and not on float_widget.
bool twindow::point_in_normal_widget(const int x, const int y, const twidget& widget) const
{
	// @widget must be not float_widget.
	VALIDATE(!widget.float_widget(), null_str);

	if (!point_in_rect(x, y, widget.get_rect())) {
		return false;
	}
	const twidget* find = float_widget_find_at(tpoint(x, y), true);
	if (find) {
		// vertical scrollbar/horizontal scrollbar is in scroll_container. think true.
		if (dynamic_cast<const tscrollbar_*>(find)) {
			find = nullptr;
		}
	}
	return find? false: true;
}

bool twindow::popup_new_window()
{
	remove_tooltip();
	draw_float_widgets();

	redraw_ |= tcontainer_::popup_new_window();

	return redraw_;
}

void twindow::init_linked_size_group(const std::string& id,
		const bool fixed_width, const bool fixed_height)
{
	VALIDATE(fixed_width || fixed_height, null_str);
	VALIDATE(!has_linked_size_group(id), null_str);

	linked_size_[id] = tlinked_size(fixed_width, fixed_height);
}

bool twindow::has_linked_size_group(const std::string& id)
{
	return linked_size_.find(id) != linked_size_.end();
}

void twindow::add_linked_widget(const std::string& id, twidget& widget, bool linked_group_only)
{
	VALIDATE(!linked_group_only, null_str);
	std::stringstream err;
	err << "not defiend group id: " << id;
	VALIDATE(has_linked_size_group(id), err.str());

	std::vector<twidget*>& widgets = linked_size_[id].widgets;
	if (std::find(widgets.begin(), widgets.end(), &widget) == widgets.end()) {
		widgets.push_back(&widget);
	}
}

void twindow::remove_linked_widget(const std::string& id, const twidget* widget)
{
	VALIDATE(widget, null_str);
	if (!has_linked_size_group(id)) {
		return;
	}

	std::vector<twidget*>& widgets = linked_size_[id].widgets;

	std::vector<twidget*>::iterator itor =
			std::find(widgets.begin(), widgets.end(), widget);

	if (itor != widgets.end()) {
		widgets.erase(itor);

		VALIDATE(std::find(widgets.begin(), widgets.end(), widget) == widgets.end(), null_str);
	}
}

void twindow::layout_init(bool linked_group_only)
{
	tlink_group_owner_lock lock(*this);
	tpanel::layout_init(linked_group_only);

	layout_linked_widgets();
}

void twindow::layout()
{
	/***** Initialize. *****/
	boost::intrusive_ptr<const twindow_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const twindow_definition::tresolution>
		(config());
	VALIDATE(conf, null_str);

	if (explicit_x_ == nposm) {
		VALIDATE(explicit_y_ == nposm, null_str);
	} else {
		VALIDATE(explicit_y_ != nposm && automatic_placement_, null_str);
	}

	get_screen_size_variables(variables_);

	// for automatic_placement_ = false, variables_ can use (volatile_width)/(volatile_height) 
	variables_.add("keyboard_height", variant(settings::keyboard_height / twidget::hdpi_scale));

	int maximum_width = settings::screen_width;
	if (!automatic_placement_ && w_.has_formula2()) {
		maximum_width = w_(variables_) * twidget::hdpi_scale;
	}

	int maximum_height = settings::screen_height;
	if (!automatic_placement_ && h_.has_formula2()) {
		maximum_height = h_(variables_) * twidget::hdpi_scale;
	}

	VALIDATE(!scene_ || (maximum_width == settings::screen_width && maximum_height == settings::screen_height), null_str);

	variables_.add("volatile_width", variant(maximum_width / twidget::hdpi_scale));
	variables_.add("volatile_height", variant(maximum_height / twidget::hdpi_scale));

	/***** Layout. *****/
	layout_init(false);

	/***** Get the best location for the window *****/
	// why don't call "tclear_restrict_width_cell_size_lock lock;"? 
	// if there is listbox, it will result to reentry in tclear_restrict_width_cell_size_lock.
	// just execute layout_init, it is safe for multiline control.
	tpoint size = get_best_size();
	
	if (automatic_placement_) {
		// resolve restrict_width's label.
		// of cause, if no restrict_width's label existed, below will increas cpu's usage. automatic placement maybe is sample.
		size = calculate_best_size_bh(size.x);
	}

	if (size.x > static_cast<int>(maximum_width) || size.y > static_cast<int>(maximum_height)) {
		std::stringstream err;

		err << _("Failed to show a dialog, which doesn't fit on the screen.");
		err << "id: " << ht::generate_format(id_, 0xffff0000);
		err << "\n";
		err << " wanted size " << get_best_size() << "\n";
		err << " available size " << maximum_width << ',' << maximum_height << "\n";
		err << " screen size " << settings::screen_width << ',' << settings::screen_height;

		throw tlayout_exception(*this, err.str());
	}

	tpoint origin(0, 0);

	if (automatic_placement_) {
		if (explicit_x_ == nposm) {
			switch (horizontal_placement_) {
				case tgrid::HORIZONTAL_ALIGN_LEFT :
					// Do nothing
					break;
				case tgrid::HORIZONTAL_ALIGN_CENTER :
					origin.x = (settings::screen_width - size.x) / 2;
					break;
				case tgrid::HORIZONTAL_ALIGN_RIGHT :
					origin.x = settings::screen_width - size.x;
					break;
				default :
					VALIDATE(false, null_str);
			}
			switch (vertical_placement_) {
				case tgrid::VERTICAL_ALIGN_TOP :
					// Do nothing
					break;
				case tgrid::VERTICAL_ALIGN_CENTER :
					origin.y = (settings::screen_height - size.y) / 2;
					break;
				case tgrid::VERTICAL_ALIGN_BOTTOM :
					origin.y = settings::screen_height - size.y;
					break;
				default :
					VALIDATE(false, null_str);
			}
		} else {
			int tmpx = (explicit_x_ / twidget::hdpi_scale) * twidget::hdpi_scale;
			if (tmpx + size.x > maximum_width) {
				tmpx = maximum_width - size.x;
			}
			origin.x = tmpx;

			int tmpy = (explicit_y_ / twidget::hdpi_scale) * twidget::hdpi_scale;
			if (tmpy + size.y > maximum_height) {
				tmpy = maximum_height - size.y;
			}
			origin.y = tmpy;
		}

	} else {
		size.x = posix_align_ceil2(size.x, twidget::hdpi_scale);
		size.y = posix_align_ceil2(size.y, twidget::hdpi_scale);

		if (w_.has_formula2()) {
			size.x = w_(variables_) * twidget::hdpi_scale;
		}
		if (h_.has_formula2()) {
			size.y = h_(variables_) * twidget::hdpi_scale;
		}

		variables_.add("width", variant(size.x / twidget::hdpi_scale));
		variables_.add("height", variant(size.y / twidget::hdpi_scale));

		origin.x = x_(variables_) * twidget::hdpi_scale;
		origin.y = y_(variables_) * twidget::hdpi_scale;

		// has calculated size, execut to calculate best size with multiline.
		tpoint new_size = calculate_best_size_bh(size.x);
		VALIDATE(new_size.x <= size.x && new_size.y <= size.y, "if new_size.y > size.y, maybe some multi-line widget result to it.");
	}

	/***** Set the window size *****/
	place(origin, size);


	need_layout_ = false;
/*
	if (id() == "studio__window_setting") {
		throw tlayout_exception(*this, null_str);
	}
*/
	if (!layouted_) {
		owner_->did_first_layouted(*this);
		layouted_ = true;
	}
}

void twindow::impl_draw_background(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	// look canvas(0) as standard canvas. animation is add to it.
	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);
}

void twindow::impl_draw_children(texture& frame_buffer, int x_offset, int y_offset)
{
	tpanel::impl_draw_children(frame_buffer, x_offset, y_offset);
}

void twindow::layout_linked_widgets()
{
	// evaluate the group sizes
	std::map<twidget*, tpoint> cache;
	typedef std::pair<const std::string, tlinked_size> hack;
	BOOST_FOREACH(hack& linked_size, linked_size_) {

		tpoint max_size(0, 0);
		cache.clear();

		// Determine the maximum size.
		BOOST_FOREACH(twidget* widget, linked_size.second.widgets) {

			if (widget->get_visible() == twidget::INVISIBLE) {
				continue;
			}

			const tpoint size = widget->get_best_size();
			cache.insert(std::make_pair(widget, size));

			if (size.x > max_size.x) {
				max_size.x = size.x;
			}
			if (size.y > max_size.y) {
				max_size.y = size.y;
			}
		}

		// Set the maximum size.
		for (std::map<twidget*, tpoint>::const_iterator it = cache.begin(); it != cache.end(); ++ it) {
			twidget* widget = it->first;
			tpoint size = it->second;

			if (linked_size.second.width) {
				size.x = max_size.x;
			}
			if (linked_size.second.height) {
				size.y = max_size.y;
			}

			widget->set_layout_size(size);
		}
	}
}

bool twindow::click_dismiss(const int x, const int y)
{
	if (does_click_dismiss()) {
		if (x == nposm || !did_click_dismiss_except_ || !did_click_dismiss_except_(x, y)) {
			set_retval(CANCEL);
			return true;
		}
	}
	return false;
}

void twindow::reset_scrollbar()
{
	const std::string horizontal_id = settings::horizontal_scrollbar_id;
	const std::string vertical_id = settings::vertical_scrollbar_id;

	tfloat_widget* vertical_scrollbar = find_float_widget(vertical_id);
	if (vertical_scrollbar->ref_widget()) {
		tscroll_container& container = *dynamic_cast<tscroll_container*>(vertical_scrollbar->ref_widget());
		container.reset_scrollbar(*this);
	}

}

void twindow::insert_tooltip(const std::string& msg, tcontrol& focus)
{
	if (msg.empty()) {
		return;
	}

	if (tooltip_at_ == nposm) {
		return;
	}
	tfloat_widget& item = *float_widgets_[tooltip_at_].get();
	item.set_visible(true);
	item.set_ref_widget(&focus, null_point);
	tcontrol& widget = *item.widget.get();
	widget.set_label(msg);
}

void twindow::remove_tooltip()
{
	if (tooltip_at_ == nposm) {
		return;
	}
	tfloat_widget& item = *float_widgets_[tooltip_at_].get();
	item.set_visible(false);
	item.set_ref_widget(nullptr, null_point);
}

void twindow::insert_float_widget(const twindow_builder::tfloat_widget& builder, twidget& widget2)
{
	tcontrol& widget = *dynamic_cast<tcontrol*>(&widget2);
	float_widgets_.push_back(std::unique_ptr<tfloat_widget>(new tfloat_widget(*this, builder, widget)));
	widget.set_parent(this);
	widget.set_visible(twidget::INVISIBLE);
	widget.set_float_widget();

	const std::string& id = widget.id();
	if (id == settings::horizontal_scrollbar_id || id == settings::vertical_scrollbar_id) {
		float_widgets_.back()->is_scrollbar = true;
		float_widgets_.back()->scrollbar_size = SCROLLBAR_BEST_SIZE;
		widget.set_icon("widgets/scrollbar");
		if (id == settings::vertical_scrollbar_id) {
			VALIDATE(dynamic_cast<tscrollbar_*>(&widget)->unrestricted_drag(), null_str);
		}

	} else if (id == settings::tooltip_id) {
		tooltip_at_ = float_widgets_.size() - 1;

	} else if (id == settings::drag_id) {
		widget.set_border("label-tooltip");
		widget.disable_allow_handle_event();
		drag_widget_ = float_widgets_.back().get();

	} else if (id == settings::magnifier_id) {
		widget.set_border("label-tooltip");
		widget.disable_allow_handle_event();

	} else if (id == settings::edit_select_all_id) {
		widget.set_label(_("Select all"));
		widget.set_border("float_widget");
		connect_signal_mouse_left_click(
			  widget
			, boost::bind(
				  &twindow::click_edit_button
				, this
				, float_widget_select_all));
	} else if (id == settings::edit_select_id) {
		widget.set_label(_("Select"));
		widget.set_border("float_widget");
		connect_signal_mouse_left_click(
			  widget
			, boost::bind(
				  &twindow::click_edit_button
				, this
				, float_widget_select));
	} else if (id == settings::edit_copy_id) {
		widget.set_label(_("Copy"));
		widget.set_border("float_widget");
		connect_signal_mouse_left_click(
			  widget
			, boost::bind(
				  &twindow::click_edit_button
				, this
				, float_widget_copy));
	} else if (id == settings::edit_paste_id) {
		widget.set_label(_("Paste"));
		widget.set_border("float_widget");
		connect_signal_mouse_left_click(
			  widget
			, boost::bind(
				  &twindow::click_edit_button
				, this
				, float_widget_paste));
	}
}

// draw tooltip to screen
void twindow::draw_float_widgets()
{
	SDL_Renderer* renderer = get_renderer();
	texture& screen = get_screen_texture();
	SDL_Rect dst;

	bool volitile_maybe_dirty = false;
	int at = 0;

	for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it, at ++) {
		tfloat_widget& item = *(it->get());
		tcontrol& widget = *(item.widget.get());

		if ((item.ref_.empty() && !item.ref_widget_) || widget.get_visible() != twidget::VISIBLE) {
			continue;
		}
		texture& buf = item.buf;
		texture& canvas = item.canvas;
		std::vector<SDL_Rect>& rects = item.buf_rects;

		SDL_Rect rect = widget.get_rect();
		if (item.need_layout || !rect.w || !rect.h) {
			tcontrol* ref = item.ref_widget_? item.ref_widget_: dynamic_cast<tcontrol*>(find(item.ref_, false));
			if (!ref) {
				std::stringstream err;
				err << "Can not find float widget(id: " << widget.id() << ")'s reference widget(id: " << item.ref_ << ")";
				VALIDATE(false, err.str()); // if false, WML result it. assert let known during as early as possible.
			}
			if (ref->get_visible() != twidget::VISIBLE) {
				continue;
			}
			if (!ref->get_width() || !ref->get_height()) {
				continue;
			}
			tpoint size = widget.get_best_size();
			// here size maybe (0, 0). it calcualted by below width/height formula.

			// SDL_Rect ref_rect = ref->get_float_widget_ref_rect();
			// if content_grid's height < conteht' height, horizontal scrollbar's y mistake. but now don't show it.
			// if according to contng_grid' height, vertical scrollbar maybe error, for chat's history message.
			SDL_Rect ref_rect = ref->get_rect();

			game_logic::map_formula_callable variables;
			variables.add("svga", variant(game_config::svga));
			variables.add("vga", variant((int)settings::screen_width >= 640 * twidget::hdpi_scale && (int)settings::screen_height >= 480 * twidget::hdpi_scale));
			variables.add("mobile", variant(game_config::mobile));
			variables.add("ref_width", variant(ref_rect.w));
			variables.add("ref_height", variant(ref_rect.h));
			variables.add("hdpi_scale", variant(twidget::hdpi_scale));

			// calculate width
			if (item.w.first != null_str) {
				gui2::tformula<int> width(item.w.first);
				size.x = width(variables);
				size.x += item.w.second * twidget::hdpi_scale;
			}

			// some reason maybe result strange result. for example keyboard_height varaible minus width height.
			if (size.x <= 0) {
				continue;
			}
			// calculate height
			if (item.h.first != null_str) {
				gui2::tformula<int> height(item.h.first);
				size.y = height(variables);
				size.y += item.h.second * twidget::hdpi_scale;
			}
			if (size.y <= 0) {
				continue;
			}
			variables.add("width", variant(size.x));
			variables.add("height", variant(size.y));
			tpoint origin(0, 0);
			for (std::vector<std::pair<std::string, int> >::const_iterator it = item.x.begin(); it != item.x.end(); ++ it) {
				if (is_null_coordinate(item.mouse_)) {
					origin.x = ref_rect.x;
				} else {
					origin.x = item.mouse_.x;
				}
				if (!it->first.empty()) {
					// it->first has contain "()"
					gui2::tformula<int> width(it->first);
					origin.x += width(variables);
				}
				origin.x += it->second * twidget::hdpi_scale;
				
				if (origin.x >= twidget::x_ && origin.x + size.x <= twidget::x_ + (int)twidget::w_) {
					break;
				}
			}

			const int os_status_height = game_config::mobile? 16 * twidget::hdpi_scale: 0;
			for (std::vector<std::pair<std::string, int> >::const_iterator it = item.y.begin(); it != item.y.end(); ++ it) {
				if (is_null_coordinate(item.mouse_)) {
					origin.y = ref_rect.y;
				} else {
					origin.y = item.mouse_.y;
				}
				if (!it->first.empty()) {
					// it->first has contain "()"
					gui2::tformula<int> height(it->first);
					origin.y += height(variables);
				}
				origin.y += it->second * twidget::hdpi_scale;

                if (origin.y < os_status_height) {
					continue;
                }
				if (origin.y >= twidget::y_ && origin.y + size.y <= twidget::y_ + (int)twidget::h_) {
					break;
				}
			}

			widget.place(origin, size);
			rect = widget.get_rect();

			SDL_Rect clip, window_rect = get_rect();
			// part of rect maybe out of window's rect. must clip inside part to rects.
			SDL_IntersectRect(&rect, &window_rect, &clip);
			if (SDL_RectEmpty(&clip)) {
				// reason same as size.x <= 0/size.y <= 0.
				buf = nullptr;
				continue;
			}
            
			int buf_width = 0, buf_height = 0;
			{
				if (buf.get()) {
					SDL_QueryTexture(buf.get(), NULL, NULL, &buf_width, &buf_height);
				}
				if (buf_width != size.x || buf_height != size.y) {
					buf = create_neutral_texture(size.x, size.y, SDL_TEXTUREACCESS_TARGET);
					SDL_SetTextureBlendMode(buf.get(), SDL_BLENDMODE_NONE);	
				}
			}

			rects.clear();
			rects.push_back(clip);

			if (!item.is_scrollbar) {
				// because glDrawArrays bug!
				// easy reappeared when use eidt_box's magnifier.
				dirty_under_rect(clip);
			}

			ref->associate_float_widget(item, true);
			item.need_layout = false;
			if (at != tooltip_at_) {
				volitile_maybe_dirty = true;
			}
		}

		if (widget.get_dirty() || widget.get_redraw()) {
			canvas = widget.get_canvas_tex();
			widget.clear_dirty();
		}

		if (!rects.empty()) {
			ttexture_blend_none_lock lock(screen);
			trender_target_lock lock2(renderer, buf);

			SDL_Rect clip;
			for (std::vector<SDL_Rect>::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
				const SDL_Rect& r = *it;

				// must clip inside part to rects.
				SDL_IntersectRect(&rect, &r, &clip);
				VALIDATE(clip == r, null_str);

				dst = ::create_rect(r.x - rect.x, r.y - rect.y, r.w, r.h);
				SDL_RenderCopy(renderer, screen.get(), &r, &dst);
			}
			rects.clear();
		}

		SDL_Rect window_rect = get_rect();
		SDL_Rect srcrect = ::create_rect(0, 0, rect.w, rect.h), dstrect;
		// part of rect maybe out of window's rect. clip inside part.
		SDL_IntersectRect(&rect, &window_rect, &dstrect);
		if (SDL_RectEmpty(&dstrect)) {
			continue;
		}
		if (dstrect != rect) {
			srcrect.x = dstrect.x - rect.x;
			srcrect.y = dstrect.y - rect.y;
			srcrect.w = dstrect.w;
			srcrect.h = dstrect.h;
		}
		SDL_RenderCopy(renderer, canvas.get(), &srcrect, &dstrect);
	}

	if (volitile_maybe_dirty && scene_) {
		set_main_map_volatiles();
	}
}

void twindow::undraw_float_widgets()
{
	SDL_Renderer* renderer = get_renderer();
	texture& screen = get_screen_texture();

	for (std::vector<std::unique_ptr<tfloat_widget> >::const_reverse_iterator it = float_widgets_.rbegin(); it != float_widgets_.rend(); ++it) {
		const tfloat_widget& item = *(it->get());
		const tcontrol& widget = *item.widget;
		
		if ((item.ref_.empty() && !item.ref_widget_) || widget.get_visible() != twidget::VISIBLE) {
			continue;
		}
		const tcontrol* ref = item.ref_widget_? item.ref_widget_: dynamic_cast<tcontrol*>(find(item.ref_, false));
		if (ref->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (!ref->get_width() || !ref->get_height()) {
			continue;
		}
		SDL_Rect rect = widget.get_rect();
		if (rect.w <= 0 || rect.h <= 0) {
			continue;
		}
		texture& buf = (*it)->buf;
		if (buf.get()) {
			ttexture_blend_none_lock lock(buf);
			SDL_Rect window_rect = get_rect();
			SDL_Rect srcrect = ::create_rect(0, 0, rect.w, rect.h), dstrect;
			// part of rect maybe out of window's rect. clip inside part.
			SDL_IntersectRect(&rect, &window_rect, &dstrect);
			if (SDL_RectEmpty(&dstrect)) {
				continue;
			}
			if (dstrect != rect) {
				srcrect.x = dstrect.x - rect.x;
				srcrect.y = dstrect.y - rect.y;
				srcrect.w = dstrect.w;
				srcrect.h = dstrect.h;
			}
			SDL_RenderCopy(renderer, buf.get(), &srcrect, &dstrect);
		}
	}
}

tfloat_widget* twindow::find_float_widget(const std::string& id) const
{
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it) {
		tfloat_widget& item = *(it->get());
		if (item.widget->id() == id) {
			return &item;
		}
	}
	return nullptr;
}

void twindow::set_main_map_volatiles()
{
	VALIDATE(scene_, null_str);

	const twidget* widget = find("_main_map", false);
	SDL_Rect map_area = widget->get_rect();

	std::vector<twidget*> result;
	int at = 0;
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it, at ++) {
		if (at == tooltip_at_) {
			continue;
		}
		tfloat_widget& item = *(it->get());
		tcontrol* widget = item.widget.get();
		if (widget->get_visible() != twidget::VISIBLE) {
			continue;
		}
		if (rects_overlap(widget->get_rect(), map_area)) {
			result.push_back(widget);
			// widget->set_volatile(true);
		}
	}

	dialog()->set_volatiles(result);
}

void twindow::clear_texture()
{
	for (std::vector<std::unique_ptr<tfloat_widget> >::const_iterator it = float_widgets_.begin(); it != float_widgets_.end(); ++it) {
		tfloat_widget& item = *(it->get());
		item.buf = nullptr;
		item.canvas = nullptr;
		item.widget->clear_texture();
	}
	tpanel::clear_texture();
}

const std::string& twindow::get_control_type() const
{
	static const std::string type = "window";
	return type;
}

void twindow::mouse_capture(twidget* widget)
{
	VALIDATE(event_distributor_, null_str);
	event_distributor_->capture_mouse(widget);
}

twidget* twindow::mouse_captured_widget() const
{
	VALIDATE(event_distributor_, null_str);
	return event_distributor_->mouse_captured_widget();
}

void twindow::clear_mouse_click() const
{
	VALIDATE(event_distributor_, null_str);
	return event_distributor_->clear_mouse_click();
}

twidget* twindow::mouse_click_widget() const
{
	VALIDATE(event_distributor_, null_str);
	return event_distributor_->mouse_click_widget();
}

void twindow::keyboard_capture(twidget* widget)
{
	VALIDATE(event_distributor_, null_str);
	event_distributor_->keyboard_capture(widget);
}

twidget* twindow::keyboard_capture_widget() const
{
	VALIDATE(event_distributor_, null_str);
	return event_distributor_->keyboard_capture_widget();
}

void twindow::set_out_of_chain_widget(twidget* widget)
{
	VALIDATE(event_distributor_, null_str);
	event_distributor_->set_out_of_chain_widget(widget);
}

void twindow::add_to_keyboard_chain(twidget* widget)
{
	VALIDATE(event_distributor_, null_str);
	event_distributor_->keyboard_add_to_chain(widget);
}

void twindow::remove_from_keyboard_chain(twidget* widget)
{
	VALIDATE(event_distributor_, null_str);
	event_distributor_->keyboard_remove_from_chain(widget);
}

void twindow::signal_handler_sdl_video_resize(const event::tevent event, bool& handled, const tpoint& new_size)
{
	const tpoint new_size2(posix_align_floor(new_size.x, twidget::hdpi_scale), posix_align_floor(new_size.y, twidget::hdpi_scale));

	if (new_size2.x == static_cast<int>(settings::screen_width)	&& new_size2.y == static_cast<int>(settings::screen_height)) {
		// resize not needed.
		handled = true;
		return;
	}

	if (!preferences::set_resolution(video_, new_size2.x, new_size2.y)) {
		// resize aborted, resize failed.
		return;
	}

	settings::screen_width = new_size2.x;
	settings::screen_height = new_size2.y;
	invalidate_layout(nullptr);

	handled = true;
}

void twindow::signal_handler_mouse_motion(const tpoint& coordinate)
{
	if (did_drag_mouse_motion_) {
		VALIDATE(drag_widget_->widget->get_visible() == twidget::VISIBLE, null_str);
		drag_widget_->set_ref_widget(this, tpoint(coordinate.x, coordinate.y));

		bool continu = did_drag_mouse_motion_(coordinate.x, coordinate.y);
		if (!continu) {
			stop_drag();
		}
	}
}

void twindow::signal_handler_mouse_leave(const tpoint& coordinate, const tpoint& coordinate2)
{
	if (did_drag_mouse_leave_ && (is_magic_coordinate(coordinate) || is_up_result_leave(coordinate2))) {
		if (!is_up_result_leave(coordinate2) && !is_magic_coordinate(coordinate)) {
			// normal, form widget to other widget.
			return;
		}
		if (is_up_result_leave(coordinate2)) {
			VALIDATE(!is_magic_coordinate(coordinate), null_str);
		}
		// did_drag_mouse_leave_(coordinate.x, coordinate.y, is_up_result_leave(coordinate2));
		tdialog::tmsg_data_drag_mouse_leave* pdata = new tdialog::tmsg_data_drag_mouse_leave(did_drag_mouse_leave_, coordinate.x, coordinate.y, is_up_result_leave(coordinate2));
		rtc::Thread::Current()->Post(RTC_FROM_HERE, owner_, tdialog::POST_MSG_DRAG_MOUSE_LEAVE, pdata);

		stop_drag();
	}
}

void twindow::set_drag_surface(const surface& surf, const bool border)
{
	// must call set_drag_surface before start_drag.
	VALIDATE(surf, null_str);

	tcontrol& widget = *drag_widget_->widget.get();
	VALIDATE(widget.get_visible() == twidget::INVISIBLE, "must call set_drag_surface before start_drag.");

	std::stringstream x_ss, y_ss, width_ss, height_ss;
	tpoint border_size(0, 0);
	if (border) {
		border_size.x = widget.config()->text_extra_width;
		border_size.y = widget.config()->text_extra_height;

		width_ss << "(width)";
		height_ss << "(height)";
	} else {
		x_ss << "(" << (-1 * widget.config()->text_extra_width / 2) << ")";
		y_ss << "(" << (-1 * widget.config()->text_extra_height / 2) << ")";
		width_ss << "(width + " << widget.config()->text_extra_width << ")";
		height_ss << "(height + " << widget.config()->text_extra_height << ")";

		widget.set_border(null_str);
	}

	const tpoint best_size(surf->w + border_size.x, surf->h + border_size.y);

	widget.set_blits(tformula_blit(surf, x_ss.str(), y_ss.str(), width_ss.str(), height_ss.str()));
	widget.set_layout_size(best_size);
}

void twindow::start_drag(const tpoint& coordinate, const boost::function<bool (const int, const int)>& did_mouse_motion, 
		const boost::function<void (const int, const int, bool)>& did_mouse_leave)
{
	VALIDATE(did_mouse_leave, null_str);

	tcontrol& widget = *drag_widget_->widget.get();
	VALIDATE(widget.get_visible() == twidget::INVISIBLE, null_str);

	drag_widget_->set_ref_widget(this, tpoint(coordinate.x, coordinate.y));
	drag_widget_->set_visible(true);

	did_drag_mouse_motion_ = did_mouse_motion;
	did_drag_mouse_leave_ = did_mouse_leave;
}

void twindow::stop_drag()
{
	tcontrol& widget = *drag_widget_->widget.get();

	VALIDATE(widget.get_visible() == twidget::VISIBLE, null_str);
	drag_widget_->set_visible(false);

	did_drag_mouse_motion_ = NULL;
	did_drag_mouse_leave_ = NULL;

	// app maybe change border during last drag.
	widget.set_border("label-tooltip");
}

void twindow::signal_handler_click_dismiss(
		const event::tevent event, bool& handled, bool& halt)
{
	if (is_closing()) {
		return;
	}
	handled = halt = click_dismiss();
}

void twindow::signal_handler_click_dismiss2(bool& handled, bool& halt, const tpoint& coordinate)
{
	if (!click_dismiss_) {
		return;
	}
	if (!find_at(coordinate, true)) {
		// not over a control.
		handled = halt = click_dismiss(coordinate.x, coordinate.y);
	}
}

bool twindow::signal_handler_sdl_mouse_motion(const tpoint& coordinate)
{
	bool handled = false;
	// in window's rect, think not dismiss always.
	if (leave_dismiss_) {
		bool ret = false;
		if (did_leave_dismiss_) {
			ret = did_leave_dismiss_(coordinate.x, coordinate.y);
		} else {
			ret = !point_in_rect(coordinate.x, coordinate.y, get_rect());
		}
		if (ret) {
			set_retval(CANCEL);
			handled = true;
		}
	}
	return handled;
}

void twindow::signal_handler_sdl_key_down(
		const event::tevent event, bool& handled, SDL_Keycode key)
{
	if (key == SDLK_ESCAPE && !escape_disabled_) {
		set_retval(CANCEL);
		handled = true;
	} else if (key == SDLK_SPACE) {
		handled = click_dismiss();
	}
}

void twindow::signal_handler_message_show_tooltip(
		  const event::tevent event
		, bool& handled
		, event::tmessage& message)
{
	event::tmessage_show_tooltip& request =
			*dynamic_cast<event::tmessage_show_tooltip*>(&message);

	tcontrol* focus = dynamic_cast<tcontrol*>(const_cast<twidget*>(&request.widget));
	if (focus) {
		insert_tooltip(request.message, *focus);
	}

	handled = true;
}

void twindow::signal_handler_request_placement(
		  const event::tevent event
		, bool& handled)
{
	invalidate_layout(nullptr);

	handled = true;
}

} // namespace gui2


/**
 * @page layout_algorithm Layout algorithm
 *
 * @section introduction Introduction
 *
 * This page describes how the layout engine for the dialogs works. First
 * a global overview of some terms used in this document.
 *
 * - @ref gui2::twidget "Widget"; Any item which can be used in the widget
 *   toolkit. Not all widgets are visible. In general widgets can not be
 *   sized directly, but this is controlled by a window. A widget has an
 *   internal size cache and if the value in the cache is not equal to 0,0
 *   that value is its best size. This value gets set when the widget can
 *   honour a resize request.  It will be set with the value which honors
 *   the request.
 *
 * - @ref gui2::tgrid "Grid"; A grid is an invisible container which holds
 *   one or more widgets.  Several widgets have a grid in them to hold
 *   multiple widgets eg panels and windows.
 *
 * - @ref gui2::tgrid::tchild "Grid cell"; Every widget which is in a grid is
 *   put in a grid cell. These cells also hold the information about the gaps
 *   between widgets the behaviour on growing etc. All grid cells must have a
 *   widget inside them.
 *
 * - @ref gui2::twindow "Window"; A window is a top level item which has a
 *   grid with its children. The window handles the sizing of the window and
 *   makes sure everything fits.
 *
 * - @ref gui2::twindow::tlinked_size "Shared size group"; A shared size
 *   group is a number of widgets which share width and or height. These
 *   widgets are handled separately in the layout algorithm. All grid cells
 *   width such a widget will get the same height and or width and these
 *   widgets won't be resized when there's not enough size. To be sure that
 *   these widgets don't cause trouble for the layout algorithm, they must be
 *   in a container with scrollbars so there will always be a way to properly
 *   layout them. The engine must enforce this restriction so the shared
 *   layout property must be set by the engine after validation.
 *
 * - All visible grid cells; A grid cell is visible when the widget inside
 *   of it doesn't have the state INVISIBLE. Widgets which are HIDDEN are
 *   sized properly since when they become VISIBLE the layout shouldn't be
 *   invalidated. A grid cell that's invisible has size 0,0.
 *
 * - All resizable grid cells; A grid cell is resizable under the following
 *   conditions:
 *   - The widget is VISIBLE.
 *   - The widget is not in a shared size group.
 *
 * There are two layout algorithms with a different purpose.
 *
 * - The Window algorithm; this algorithm's goal is it to make sure all grid
 *   cells fit in the window. Sizing the grid cells depends on the widget
 *   size as well, but this algorithm only sizes the grid cells and doesn't
 *   handle the widgets inside them.
 *
 * - The Grid algorithm; after the Window algorithm made sure that all grid
 *   cells fit this algorithm makes sure the widgets are put in the optimal
 *   state in their grid cell.
 *
 * @section layout_algorithm_window Window
 *
 * Here is the algorithm used to layout the window:
 *
 * - Perform a full initialization
 *   (@ref gui2::twidget::layout_init ()):
 *   - Clear the internal best size cache for all widgets.
 *   - For widgets with scrollbars hide them unless the
 *     @ref gui2::tscroll_container::tscrollbar_mode "scrollbar_mode" is
 *     always_visible or auto_visible.
 * - Handle shared sizes:
 *   - Height and width:
 *     - Get the best size for all widgets that share height and width.
 *     - Set the maximum of width and height as best size for all these
 *       widgets.
 *   - Width only:
 *     - Get the best width for all widgets which share their width.
 *     - Set the maximum width for all widgets, but keep their own height.
 *   - Height only:
 *     - Get the best height for all widgets which share their height.
 *     - Set the maximum height for all widgets, but keep their own width.
 * - Start layout loop:
 *   - Get best size.
 *   - If width <= maximum_width && height <= maximum_height we're done.
 *   - If width > maximum_width, optimize the width:
 *     - For every grid cell in a grid row there will be a resize request
 *       (@ref gui2::tgrid::reduce_width):
 *       - Sort the widgets in the row on the resize priority.
 *         - Loop through this priority queue until the row fits
 *           - If priority != 0 try to share the extra width else all
 *             widgets are tried to reduce the full size.
 *           - Try to shrink the widgets by either wrapping or using a
 *             scrollbar (@ref gui2::twidget::request_reduce_width).
 *           - If the row fits in the wanted width this row is done.
 *           - Else try the next priority.
 *         - All priorities done and the width still doesn't fit.
 *         - Loop through this priority queue until the row fits.
 *           - If priority != 0:
 *             - try to share the extra width
 *           -Else:
 *             - All widgets are tried to reduce the full size.
 *           - Try to shrink the widgets by sizing them smaller as really
 *             wanted (@ref gui2::twidget::demand_reduce_width).
 *             For labels, buttons etc. they get ellipsized.
 *           - If the row fits in the wanted width this row is done.
 *           - Else try the next priority.
 *         - All priorities done and the width still doesn't fit.
 *         - Throw a layout width doesn't fit exception.
 *   - If height > maximum_height, optimize the height
 *       (@ref gui2::tgrid::reduce_height):
 *     - For every grid cell in a grid column there will be a resize request:
 *       - Sort the widgets in the column on the resize priority.
 *         - Loop through this priority queue until the column fits:
 *           - If priority != 0 try to share the extra height else all
 *              widgets are tried to reduce the full size.
 *           - Try to shrink the widgets by using a scrollbar
 *             (@ref gui2::twidget::request_reduce_height).
 *             - If succeeded for a widget the width is influenced and the
 *               width might be invalid.
 *             - Throw a width modified exception.
 *           - If the column fits in the wanted height this column is done.
 *           - Else try the next priority.
 *         - All priorities done and the height still doesn't fit.
 *         - Loop through this priority queue until the column fits.
 *           - If priority != 0 try to share the extra height else all
 *             widgets are tried to reduce the full size.
 *           - Try to shrink the widgets by sizing them smaller as really
 *             wanted (@ref gui2::twidget::demand_reduce_width).
 *             For labels, buttons etc. they get ellipsized .
 *           - If the column fits in the wanted height this column is done.
 *           - Else try the next priority.
 *         - All priorities done and the height still doesn't fit.
 *         - Throw a layout height doesn't fit exception.
 * - End layout loop.
 *
 * - Catch @ref gui2::tlayout_exception_width_modified "width modified":
 *   - Goto relayout.
 *
 * - Catch
 *   @ref gui2::tlayout_exception_width_resize_failed "width resize failed":
 *   - If the window has a horizontal scrollbar which isn't shown but can be
 *     shown.
 *     - Show the scrollbar.
 *     - goto relayout.
 *   - Else show a layout failure message.
 *
 * - Catch
 *   @ref gui2::tlayout_exception_height_resize_failed "height resize failed":
 *   - If the window has a vertical scrollbar which isn't shown but can be
 *     shown:
 *     - Show the scrollbar.
 *     - goto relayout.
 *   - Else:
 *     - show a layout failure message.
 *
 * - Relayout:
 *   - Initialize all widgets
 *     (@ref gui2::twidget::layout_init ())
 *   - Handle shared sizes, since the reinitialization resets that state.
 *   - Goto start layout loop.
 *
 * @section grid Grid
 *
 * This section will be documented later.
 */
