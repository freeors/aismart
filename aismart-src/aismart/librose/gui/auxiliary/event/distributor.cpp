/* $Id: distributor.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/auxiliary/event/distributor.hpp"

#include "events.hpp"
#include "gui/widgets/timer.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/widget.hpp"
#include "gui/widgets/window.hpp"

#include <boost/bind.hpp>


#ifdef _WIN32
#define STRICT_MODEL
#endif

namespace gui2{

namespace event {

/**
 * Small helper to keep a resource (boolean) locked.
 *
 * Some of the event handling routines can't be called recursively, this due to
 * the fact that they are attached to the pre queue and when the forward an
 * event the pre queue event gets triggered recursively causing infinite
 * recursion.
 *
 * To prevent that those functions check the lock and exit when the lock is
 * held otherwise grab the lock here.
 */
class tlock
{
public:
	tlock(bool& locked)
		: locked_(locked)
	{
		assert(!locked_);
		locked_ = true;
	}

	~tlock()
	{
		assert(locked_);
		locked_ = false;
	}
private:
	bool& locked_;
};

// capture_mouse make sure set one Widget to captured. if widget is nullptr, Widget is mouse_focus_, else is widget.
void tdistributor::capture_mouse(twidget* widget)
{
	VALIDATE(mouse_focus_, null_str);

	const twidget* original_capture_mouse = mouse_captured_widget();
	if (!widget) {
		// will use mouse_focus_ to capture.
		VALIDATE(!mouse_captured_, null_str);
	} else {
		// will use widget to capture.
		VALIDATE(widget != original_capture_mouse, null_str);
	}

	mouse_captured_ = true;

	twidget* widget2 = widget? widget: mouse_focus_;
	if (widget2 != mouse_focus_) {
		int x, y;
		SDL_GetMouseState(&x, &y);
		// x,y maybe not in mouse_focus_. why? there are motion when last motion and this capture_mouse.
		VALIDATE(enter_leave_ == 1, null_str);

		mouse_leave(tpoint(x, y), null_point);

		mouse_focus_ = widget2;
		mouse_enter(mouse_focus_, tpoint(x, y));
	}
}

twidget* tdistributor::mouse_captured_widget() const
{
	if (!mouse_captured_) {
		return nullptr;
	}
	return mouse_focus_;
}

void tdistributor::clear_mouse_click()
{
	if (mouse_click_) {
		mouse_click_ = nullptr;
	}
}

void tdistributor::signal_handler_sdl_mouse_motion(const tpoint& coordinate, const tpoint& coordinate2)
{
	VALIDATE(!signal_handler_sdl_mouse_motion_entered_, null_str);
	tlock lock(signal_handler_sdl_mouse_motion_entered_);

	const twidget* parent = mouse_focus_? mouse_focus_->parent(): nullptr;
/*
	SDL_Log("tdistributor::signal_handler_sdl_mouse_motion------(%i, %i), mouse_captured_[%s, mouse_focus_[%p(%s), parent[(%s)\n", 
		coordinate.x, coordinate.y, 
		mouse_captured_? "true": "false",
		mouse_focus_, mouse_focus_ && !mouse_focus_->id().empty()? mouse_focus_->id().c_str(): "<nil>",
		parent && !parent->id().empty()? parent->id().c_str(): "<nil>");
*/
#ifdef STRICT_MODEL
	if (is_focus_lost_event(coordinate)) {
#else
	// when switch between portrait and landscape, android will receive leave_window.
	if (is_focus_lost_event(coordinate) || is_mouse_leave_window_event(coordinate)) {
#endif
		// SDL_Log("------signal_handler_sdl_mouse_motion, magic event(%i, %i), mouse_focus_: %p\n", coordinate.x, coordinate.y, mouse_focus_);

		if (mouse_focus_) {
			mouse_leave(coordinate, null_point);
		}
		leave_window_ = false;
		post_mouse_up();
		return;
	}

	if (leave_window_) {
		// I think that it a SDL BUG. receive tow leave_window continue.
		if (is_mouse_leave_window_event(coordinate)) {
			return;
		}
		// SDL_Log("signal_handler_sdl_mouse_motion, when leave_window_ = true, receive (%i, %i)\n", coordinate.x, coordinate.y);
		// here, leave_window_=true. 1)leave window, 2)reenter window.
		VALIDATE(!is_mouse_leave_window_event(coordinate), null_str);
		leave_window_ = false;

	} else if (is_mouse_leave_window_event(coordinate)) {
		leave_window_ = true;
	}
        
	// in order to coordinate with up/down/wheel
	twidget* mouse_over = owner_.find_at(coordinate, true);
	if (mouse_click_ && mouse_over != mouse_click_) {
		// if over widget isn't the mouse_click_, set mouse_click_ to nullptr.
		// avoid leave-reenter when mouse_captured_ = true.
		mouse_click_ = nullptr;
	}

	if (mouse_captured_) {
		VALIDATE(mouse_focus_, null_str);
		if (!is_mouse_leave_window_event(coordinate)) {
			mouse_motion(mouse_focus_, coordinate, coordinate2);
		} else {
			mouse_captured_ = false;
			VALIDATE(leave_window_, null_str);
			mouse_leave(coordinate, null_point);			
		}

	} else if (mouse_focus_) {
		if (mouse_focus_ == mouse_over) {
			mouse_motion(mouse_over, coordinate, coordinate2);
		} else {
			// moved from one widget to the next
			if (mouse_focus_ != mouse_over) {
				mouse_leave(coordinate, null_point);
			}
			if (mouse_over) {
				mouse_enter(mouse_over, coordinate);
			}
		}
	} else if (mouse_over) {
		mouse_enter(mouse_over, coordinate);

	}
}

void tdistributor::signal_handler_sdl_wheel(
		  const event::tevent event
		, bool& handled
		, const tpoint& coordinate
		, const tpoint& coordinate2)
{
	twidget* target = nullptr;
	if (mouse_captured_) {
		VALIDATE(mouse_focus_, null_str);
		target = mouse_focus_;
	} else {
		target = owner_.find_at(coordinate, true);
	}

	if (target) {
		if (out_of_chain_widget_ && is_out_of_chain_widget(*target)) {
			owner_.fire(OUT_OF_CHAIN, *out_of_chain_widget_, event, target);
		}
		owner_.fire(event, *target, coordinate, coordinate2);
	}

	// caculate mouse_focus_ again.
	signal_handler_sdl_mouse_motion(coordinate, tpoint(0, 0));
	handled = true;
}

void tdistributor::signal_handler_sdl_longpress(const tpoint& coordinate)
{
	twidget* mouse_over = owner_.find_at(coordinate, true);
	if (!mouse_over) {
		return;
	}
	// once coordinate form widgetA to widgetB, mouse_click_ will be set to nullptr.
	owner_.fire(event::LONGPRESS, *mouse_over, coordinate, null_point);
}

void tdistributor::mouse_enter(twidget* mouse_over, const tpoint& coordinate)
{
	// SDL_Log("mouse_enter: %s(%p), (%i, %i, %i, %i)\n", mouse_over->id().empty()? "<nil>": mouse_over->id().c_str(), mouse_over, mouse_over->get_x(), mouse_over->get_y(), mouse_over->get_width(), mouse_over->get_height());

	VALIDATE(!enter_leave_ ++, null_str);

	VALIDATE(mouse_over, null_str);

	mouse_focus_ = mouse_over;
	owner_.fire(event::MOUSE_ENTER, *mouse_over, coordinate, null_point);

	hover_shown_ = false;
	start_hover_timer(mouse_over, get_mouse_position());
}

void tdistributor::mouse_leave(const tpoint& coordinate, const tpoint& coordinate2)
{
	// SDL_Log("mouse_leave: %s(%p), (%i, %i, %i, %i)\n", mouse_focus_->id().empty()? "<nil>": mouse_focus_->id().c_str(), mouse_focus_, mouse_focus_->get_x(), mouse_focus_->get_y(), mouse_focus_->get_width(), mouse_focus_->get_height());
    VALIDATE(mouse_focus_, null_str);

	owner_.fire(event::MOUSE_LEAVE, *mouse_focus_, coordinate, coordinate2);
	VALIDATE(mouse_focus_, "must not popup new window during MOUSE_LEAVE. replace with app_OnMessage.");
		
	owner_.fire2(NOTIFY_REMOVE_TOOLTIP, *mouse_focus_);

	mouse_focus_ = nullptr;

	stop_hover_timer();

	VALIDATE(enter_leave_ == 1, null_str);
	enter_leave_ --;
}

void tdistributor::mouse_motion(twidget* mouse_over, const tpoint& coordinate, const tpoint& coordinate2)
{
	VALIDATE(mouse_over, null_str);
/*
	SDL_Log("tdistributor::mouse_motion, coordinate(%i, %i), mouse_captured_ = %s, mouse_over: %s\n", 
		coordinate.x, coordinate.y, mouse_captured_? "true": "false",
		mouse_over->id().empty()? "<nil>": mouse_over->id().c_str());
*/
	owner_.fire(event::MOUSE_MOTION, *mouse_over, coordinate, coordinate2);

	if (hover_timer_.valid()) {
		if((abs(hover_position_.x - coordinate.x) > 5)
				|| (abs(hover_position_.y - coordinate.y) > 5)) {

			stop_hover_timer();
			start_hover_timer(mouse_over, coordinate);
		}
	}
}

void tdistributor::show_tooltip()
{
	VALIDATE(hover_timer_.valid(), null_str);
	hover_timer_.reset();

	if (!hover_widget_) {
		// See tmouse_motion::stop_hover_timer.
		// (event::SHOW_TOOLTIP) bailing out, no hover widget.;
		return;
	}

	/*
	 * Ignore the result of the event, always mark the tooltip as shown. If
	 * there was no handler, there is no reason to assume there will be one
	 * next time.
	 */
	owner_.fire(SHOW_TOOLTIP, *hover_widget_, hover_position_, null_point);

	hover_shown_ = true;

	hover_widget_ = NULL;
	hover_position_ = tpoint(0, 0);
}

void tdistributor::start_hover_timer(twidget* widget, const tpoint& coordinate)
{
	VALIDATE(widget, null_str);
	stop_hover_timer();

	if (hover_shown_ || !widget->wants_mouse_hover()) {
		return;
	}

	hover_timer_.reset(50, *static_cast<twindow*>(&owner_), boost::bind(&tdistributor::show_tooltip, this));

	if (hover_timer_.valid()) {
		hover_widget_ = widget;
		hover_position_ = coordinate;
	} else {
		// Failed to add hover timer.;
	}
}

void tdistributor::stop_hover_timer()
{
	if (hover_timer_.valid()) {
		VALIDATE(hover_widget_, null_str);
		// Stop hover timer for widget(hover_widget_->id()) at address(hover_widget_)
		hover_timer_.reset();

		hover_widget_ = NULL;
		hover_position_ = tpoint(0, 0);
	}
}

/***** ***** ***** ***** tmouse_button ***** ***** ***** ***** *****/

tdistributor::tmouse_button::tmouse_button(const tevent _button_down, const tevent _button_up, const tevent _button_click, const tevent _button_double_click,
	tdistributor& distributor, const int button, twidget& owner, const tdispatcher::tposition queue_position)
	: button_down(_button_down)
	, button_up(_button_up)
	, button_click(_button_click)
	, button_double_click(_button_double_click)
	, distributor_(distributor)
	, button_(button)
	, down_coordinate_(-1, -1)
	, last_click_stamp_(0)
	, last_clicked_widget_(NULL)
	, times_(0)
{
}

tdistributor::tmouse_button::~tmouse_button()
{
}

void tdistributor::tmouse_button::signal_handler_sdl_button_down(
		  bool& handled
		, const tpoint& coordinate)
{
	const int times = times_ ++;
/*
	SDL_Log("[#%i]signal_handler_sdl_button_down------(%i, %i), mouse_focus_: %p, mouse_click_: %p, window_id(%s)\n",
		times, coordinate.x, coordinate.y, distributor_.mouse_focus_, distributor_.mouse_click_,
		distributor_.owner_.id().empty()? "nill": distributor_.owner_.id().c_str());
*/
	VALIDATE(coordinate.x >= 0 && coordinate.y >= 0, null_str);
	VALIDATE(!distributor_.leave_window_, null_str);

	if (!distributor_.mouse_focus_) {
		VALIDATE(!distributor_.enter_leave_, null_str);
		// 1. mouse on A's [a] widget, so mouse_focus = [a].
		// 2. ([a]'s mouse_focus=nullptr, see window_connect_disconnected) popup B. mouse motion, and result B closed.
		// 3. down immediate! because no moiton, tevent_dispatcher doen't send extra motion, but A's mouse_focus_=nullptr.
		// to solve it, we can send extra motion to A, but it will result to more problem.
		// SDL_Log("distributor_.mouse_focus_ is nullptr, we hope motion conitnue, so do nothing\n");
		return;
	}

	if (button_ == SDL_BUTTON_LEFT) {
		// mouse_captured_ is only usable to SDL_BUTTON_LEFT.
		VALIDATE(!distributor_.mouse_captured_, null_str);
	}

	twidget* mouse_over = distributor_.owner_.find_at(coordinate, true);
	if (!mouse_over) {
		// SDL_Log("------[#%i]signal_handler_sdl_button_down[#%i], mouse_over = nullptr, window_id(%s)\n", times, button_, distributor_.owner_.id().empty()? "nill": distributor_.owner_.id().c_str());
		return;
	}

	if (button_ == SDL_BUTTON_LEFT) {
        VALIDATE(distributor_.mouse_focus_, null_str);
		if (mouse_over != distributor_.mouse_focus_) {
			// 1. mouse motion at (639, 595), mouse_focus_ = listbox. visible vertical scrollbar widget, this scrollbar cover (639, 595).
			// 2. left button down at (639, 595), mouse_over is scrollbar, and mouse_over != mouse_focus_.
			distributor_.mouse_leave(coordinate, null_point);
			distributor_.mouse_enter(mouse_over, coordinate);
		}
		distributor_.mouse_click_ = mouse_over;
	}

	down_coordinate_ = coordinate;
	if (distributor_.out_of_chain_widget_ && distributor_.is_out_of_chain_widget(*mouse_over)) {
		distributor_.owner_.fire(OUT_OF_CHAIN, *distributor_.out_of_chain_widget_, button_down, mouse_over);
	}
	distributor_.owner_.fire(button_down, *mouse_over, coordinate, null_point);

	handled = true;

	// SDL_Log("------[#%i]signal_handler_sdl_button_down[#%i], window_id(%s)\n", times, button_, distributor_.owner_.id().empty()? "nill": distributor_.owner_.id().c_str());
}

void tdistributor::tmouse_button::signal_handler_sdl_button_up(
		  bool& handled
		, const tpoint& coordinate)
{
	const int times = times_ ++;
/*
	SDL_Log("[#%i]signal_handler_sdl_button_up[#%i]------mouse_focus_: %p(%s), window_id(%s)\n",
		times, button_,
		distributor_.mouse_focus_, distributor_.mouse_focus_ && !distributor_.mouse_focus_->id().empty()? distributor_.mouse_focus_->id().c_str(): "<nil>",
		distributor_.owner_.id().empty()? "nill": distributor_.owner_.id().c_str());
*/
	VALIDATE(!distributor_.leave_window_, null_str);

	VALIDATE(coordinate.x >= 0 && coordinate.y >= 0 || is_multi_gestures_up(coordinate), null_str);
    // in order to couple enter/leave, let multi_gestures_up continue.
    
	twidget* mouse_over = distributor_.owner_.find_at(coordinate, true);

	twidget* up_target = mouse_over;
	if (button_ == SDL_BUTTON_LEFT && distributor_.mouse_captured_) {
		up_target = distributor_.mouse_focus_;
	}
	if (up_target) {
		if (distributor_.out_of_chain_widget_ && distributor_.is_out_of_chain_widget(*up_target)) {
			distributor_.owner_.fire(OUT_OF_CHAIN, *distributor_.out_of_chain_widget_, button_up, up_target);
		}
		distributor_.owner_.fire(button_up, *up_target, coordinate, null_point);
	}

	if (button_ == SDL_BUTTON_LEFT) {
		if (distributor_.mouse_captured_) {
			distributor_.mouse_leave(coordinate, tpoint(construct_up_result_leave_coordinate()));
		
		} else if (distributor_.mouse_focus_) {
			distributor_.mouse_leave(coordinate, tpoint(construct_up_result_leave_coordinate()));
        
		} else {
			VALIDATE(!distributor_.mouse_click_, null_str);
		}

		VALIDATE(!distributor_.mouse_focus_ && !distributor_.enter_leave_, null_str);
		if (distributor_.mouse_click_ && distributor_.mouse_click_ == mouse_over) {
			mouse_button_click(*mouse_over);

			if (distributor_.mouse_focus_) {
				// did_click_ maybe enter one widget. for example in studio's scene
				// 1. click one widget on left's widget list.
				// 2. auto scroll, during auto scroll mouse maybe enter _main_map.
				int x, y;
				SDL_GetMouseState(&x, &y);
				VALIDATE(distributor_.enter_leave_ == 1 && point_in_rect(x, y, distributor_.mouse_focus_->get_rect()), null_str);
				distributor_.mouse_leave(tpoint(x, y), null_point);
			}
		}
		distributor_.post_mouse_up();
	}

	handled = true;
/*
	SDL_Log("------[#%i]signal_handler_sdl_button_up[#%i], mouse_click_: %p, window_id(%s)\n", times, button_, distributor_.mouse_click_,
		distributor_.owner_.id().empty()? "nill": distributor_.owner_.id().c_str());
*/
}

bool tdistributor::is_out_of_chain_widget(const twidget& from) const
{
	VALIDATE(out_of_chain_widget_, null_str);
	VALIDATE(out_of_chain_widget_ != &owner_, null_str);

	const twidget* widget = &from;
	if (widget == out_of_chain_widget_) {
		return false;
	}
	while (widget != &owner_) {
		widget = widget->parent();
		VALIDATE(widget, null_str);
		if (widget == out_of_chain_widget_) {
			return false;
		}
	}
	return true;
}

void tdistributor::post_mouse_up()
{
	mouse_captured_ = false;
	mouse_focus_ = nullptr;
	mouse_click_ = nullptr;
}

void tdistributor::tmouse_button::mouse_button_click(twidget& widget)
{
	Uint32 stamp = SDL_GetTicks();
	if (last_click_stamp_ + settings::double_click_time >= stamp && last_clicked_widget_ == &widget) {
		distributor_.owner_.fire(button_double_click, widget);
		last_click_stamp_ = 0;
		last_clicked_widget_ = NULL;

	} else {
		int type = twidget::drag_none;

		last_clicked_widget_ = &widget;
		distributor_.owner_.fire(button_click, widget, type);
		last_click_stamp_ = stamp;
	}
}

/***** ***** ***** ***** tdistributor ***** ***** ***** ***** *****/

/**
 * @todo Test wehether the state is properly tracked when an input blocker is
 * used.
 */
tdistributor::tdistributor(twidget& owner
		, const tdispatcher::tposition queue_position)
	: mouse_left_(LEFT_BUTTON_DOWN
			, LEFT_BUTTON_UP
			, LEFT_BUTTON_CLICK
			, LEFT_BUTTON_DOUBLE_CLICK
			, *this, SDL_BUTTON_LEFT
			, owner
			, queue_position)
	, mouse_middle_(MIDDLE_BUTTON_DOWN
			, MIDDLE_BUTTON_UP
			, MIDDLE_BUTTON_CLICK
			, MIDDLE_BUTTON_DOUBLE_CLICK
			, *this, SDL_BUTTON_MIDDLE
			, owner
			, queue_position)
	, mouse_right_(RIGHT_BUTTON_DOWN
			, RIGHT_BUTTON_UP
			, RIGHT_BUTTON_CLICK
			, RIGHT_BUTTON_DOUBLE_CLICK
			, *this, SDL_BUTTON_RIGHT
			, owner
			, queue_position)
	, owner_(owner)
	, hover_pending_(false)
	, hover_id_(0)
	, hover_box_()
	, had_hover_(false)
	, out_of_chain_widget_(nullptr)
	, keyboard_focus_(0)
	, keyboard_focus_chain_()
	, mouse_focus_(nullptr)
	, mouse_captured_(false)
	, mouse_click_(nullptr)
	, leave_window_(false)
	, hover_widget_(NULL)
	, hover_position_(0, 0)
	, hover_shown_(true)
	, signal_handler_sdl_mouse_motion_entered_(false)
	, enter_leave_(0)
{
	if(SDL_WasInit(SDL_INIT_TIMER) == 0) {
		if(SDL_InitSubSystem(SDL_INIT_TIMER) == -1) {
			assert(false);
		}
	}

	owner_.connect_signal<event::SDL_KEY_DOWN>(
			boost::bind(&tdistributor::signal_handler_sdl_key_down
				, this, _5, _6, _7));

	owner_.connect_signal<event::SDL_TEXT_INPUT>(
			boost::bind(&tdistributor::signal_handler_sdl_text_input
				, this, _5));

	owner_.connect_signal<event::NOTIFY_REMOVAL>(
			boost::bind(
				  &tdistributor::signal_handler_notify_removal
				, this
				, _1
				, _2));
}

tdistributor::~tdistributor()
{
	stop_hover_timer();

	owner_.disconnect_signal<event::SDL_KEY_DOWN>(
			boost::bind(&tdistributor::signal_handler_sdl_key_down
				, this, _5, _6, _7));
	owner_.disconnect_signal<event::SDL_TEXT_INPUT>(
			boost::bind(&tdistributor::signal_handler_sdl_text_input
				, this, _5));

	owner_.disconnect_signal<event::NOTIFY_REMOVAL>(
			boost::bind(
				  &tdistributor::signal_handler_notify_removal
				, this
				, _1
				, _2));
}

void tdistributor::keyboard_capture(twidget* widget)
{
	if (keyboard_focus_ == widget) {
		return;
	}

	if (keyboard_focus_) {
		owner_.fire2(event::LOSE_KEYBOARD_FOCUS, *keyboard_focus_);
	}

	if (widget && widget->get_active()) {
		keyboard_focus_ = widget;
	} else {
		keyboard_focus_ = nullptr;
	}

	if (keyboard_focus_) {
		owner_.fire2(event::RECEIVE_KEYBOARD_FOCUS, *keyboard_focus_);
	}
}

void tdistributor::set_out_of_chain_widget(twidget* widget)
{
	out_of_chain_widget_ = widget;
}

void tdistributor::keyboard_add_to_chain(twidget* widget)
{
	assert(widget);
	assert(std::find(keyboard_focus_chain_.begin()
				, keyboard_focus_chain_.end()
				, widget)
			== keyboard_focus_chain_.end());

	keyboard_focus_chain_.push_back(widget);
}

void tdistributor::keyboard_remove_from_chain(twidget* widget)
{
	assert(widget);
	std::vector<twidget*>::iterator itor = std::find(
		keyboard_focus_chain_.begin(), keyboard_focus_chain_.end(), widget);

	if(itor != keyboard_focus_chain_.end()) {
		keyboard_focus_chain_.erase(itor);
	}
}

void tdistributor::signal_handler_sdl_key_down(const SDL_Keycode key
		, const SDL_Keymod modifier
		, const Uint16 unicode)
{
	/** @todo Test whether recursion protection is needed. */
	if (keyboard_focus_) {
		// Attempt to cast to control, to avoid sending events if the
		// widget is disabled. If the cast fails, we assume the widget
		// is enabled and ready to receive events.
		tcontrol* control = dynamic_cast<tcontrol*>(keyboard_focus_);
		if(!control || control->get_active()) {
			if(owner_.fire(event::SDL_KEY_DOWN, *keyboard_focus_, key, modifier, unicode)) {
				return;
			}
		}
	}

	for (std::vector<twidget*>::reverse_iterator
				ritor = keyboard_focus_chain_.rbegin()
			; ritor != keyboard_focus_chain_.rend()
			; ++ritor) {

		if (*ritor == keyboard_focus_) {
			continue;
		}

		if(*ritor == &owner_) {
			/**
			 * @todo Make sure we're not in the event chain.
			 *
			 * No idea why we're here, but needs to be fixed, otherwise we keep
			 * calling this function recursively upon unhandled events...
			 *
			 * Probably added to make sure the window can grab the events and
			 * handle + block them when needed, this is no longer needed with
			 * the chain.
			 */
			continue;
		}

		// Attempt to cast to control, to avoid sending events if the
		// widget is disabled. If the cast fails, we assume the widget
		// is enabled and ready to receive events.
		tcontrol* control = dynamic_cast<tcontrol*>(keyboard_focus_);
		if (control != NULL && !control->get_active()) {
			continue;
		}

		if (owner_.fire(event::SDL_KEY_DOWN, **ritor, key, modifier, unicode)) {
			return;
		}
	}
}

void tdistributor::signal_handler_sdl_text_input(const char* text)
{
	/** @todo Test whether recursion protection is needed. */
	if (keyboard_focus_) {
		// Attempt to cast to control, to avoid sending events if the
		// widget is disabled. If the cast fails, we assume the widget
		// is enabled and ready to receive events.
		tcontrol* control = dynamic_cast<tcontrol*>(keyboard_focus_);
		if (!control || control->get_active()) {
			if(owner_.fire(event::SDL_TEXT_INPUT, *keyboard_focus_, 0, text)) {
				return;
			}
		}
	}

	for (std::vector<twidget*>::reverse_iterator
				ritor = keyboard_focus_chain_.rbegin()
			; ritor != keyboard_focus_chain_.rend()
			; ++ritor) {

		if(*ritor == keyboard_focus_) {
			continue;
		}

		if(*ritor == &owner_) {
			/**
			 * @todo Make sure we're not in the event chain.
			 *
			 * No idea why we're here, but needs to be fixed, otherwise we keep
			 * calling this function recursively upon unhandled events...
			 *
			 * Probably added to make sure the window can grab the events and
			 * handle + block them when needed, this is no longer needed with
			 * the chain.
			 */
			continue;
		}

		// Attempt to cast to control, to avoid sending events if the
		// widget is disabled. If the cast fails, we assume the widget
		// is enabled and ready to receive events.
		tcontrol* control = dynamic_cast<tcontrol*>(keyboard_focus_);
		if(control != NULL && !control->get_active()) {
			continue;
		}

		if (owner_.fire(event::SDL_TEXT_INPUT, **ritor, 0, text)) {
			return;
		}
	}
}

void tdistributor::signal_handler_notify_removal(tdispatcher& widget, const tevent event)
{
	if (out_of_chain_widget_ == &widget) {
		out_of_chain_widget_ = nullptr;
	}

	if (mouse_left_.last_clicked_widget_ == &widget) {
		mouse_left_.last_clicked_widget_ = nullptr;
	}

	if (mouse_middle_.last_clicked_widget_ == &widget) {
		mouse_middle_.last_clicked_widget_ = nullptr;
	}

	if (mouse_right_.last_clicked_widget_ == &widget) {
		mouse_right_.last_clicked_widget_ = nullptr;
	}

	if (hover_widget_ == &widget) {
		stop_hover_timer();
	}

	if (mouse_focus_ == &widget) {
		mouse_captured_ = false;
		if (enter_leave_ == 1) {
			enter_leave_ = 0;
		}
		mouse_focus_ = nullptr;
	}

	if (mouse_click_ == &widget) {
		mouse_click_ = nullptr;
	}

	if (keyboard_focus_ == &widget) {
		keyboard_focus_ = NULL;
	}
	const std::vector<twidget*>::iterator itor = std::find(
			  keyboard_focus_chain_.begin()
			, keyboard_focus_chain_.end()
			, &widget);
	if (itor != keyboard_focus_chain_.end()) {
		keyboard_focus_chain_.erase(itor);
	}
}

void tdistributor::window_connect_disconnected(bool connect)
{
	int x, y;
	SDL_GetMouseState(&x, &y);
	const tpoint coordinate(x, y);
	if (connect) {
		// SDL_Log("window_connect_disconnected: connect: %s, mouse_focus_: %p, window(%s)\n", connect? "true": "false", mouse_focus_, owner_.id().c_str());
		// will popup B, this is in A, and reset A.
		if (mouse_focus_) {
			VALIDATE(enter_leave_ == 1, null_str);
			mouse_leave(coordinate, null_point);
		}
		leave_window_ = false;
		post_mouse_up();
	}
	
}

} // namespace event

} // namespace gui2

