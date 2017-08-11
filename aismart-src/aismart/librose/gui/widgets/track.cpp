#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/track.hpp"

#include "gui/auxiliary/widget_definition/track.hpp"
#include "gui/auxiliary/window_builder/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/timer.hpp"

#include <boost/bind.hpp>

#include "rose_config.hpp"

namespace gui2 {

REGISTER_WIDGET(track)

ttrack::tdraw_lock::tdraw_lock(SDL_Renderer* renderer, ttrack& widget)
	: widget_(widget)
	, renderer_(renderer)
	, original_(SDL_GetRenderTarget(renderer))
{
	if (widget_.float_widget_) {
		texture& target = widget_.canvas(0).canvas_tex();
		SDL_Texture* tex = target.get();
		VALIDATE(tex, null_str);
		
		// if want texture to target, this texture must be SDL_TEXTUREACCESS_TARGET.
		int access;
		SDL_QueryTexture(tex, NULL, &access, NULL, NULL);
		VALIDATE(access == SDL_TEXTUREACCESS_TARGET, null_str);

		SDL_SetRenderTarget(renderer, tex);

	}

	const SDL_Rect rect = widget.get_draw_rect();
	SDL_RenderGetClipRect(renderer, &original_clip_rect_);
	SDL_RenderSetClipRect(renderer, widget_.float_widget_? nullptr: &rect);
}

ttrack::tdraw_lock::~tdraw_lock()
{
	SDL_RenderSetClipRect(renderer_, SDL_RectEmpty(&original_clip_rect_)? nullptr: &original_clip_rect_);
	if (widget_.float_widget_) {
		SDL_SetRenderTarget(renderer_, original_);
	}
}

ttrack::ttrack()
	: tcontrol(1)
	, active_(true)
	, require_capture_(true)
	, timer_interval_(0)
	, first_coordinate_(construct_null_coordinate())
{
	connect_signal<event::LEFT_BUTTON_DOWN>(boost::bind(
				&ttrack::signal_handler_left_button_down, this, _5));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttrack::signal_handler_mouse_leave, this, _5));

	connect_signal<event::MOUSE_MOTION>(boost::bind(
				&ttrack::signal_handler_mouse_motion, this, _5));
}

ttrack::~ttrack()
{
}

void ttrack::reset_background_texture(const texture& screen, const SDL_Rect& rect)
{
	if (!screen.get()) {
		VALIDATE(SDL_RectEmpty(&rect), null_str);
	} else {
		VALIDATE(!SDL_RectEmpty(&rect), null_str);
	}

	if (background_tex_) {
		int width, height;
		SDL_QueryTexture(background_tex_.get(), nullptr, nullptr, &width, &height);
		if (width != rect.w || height != rect.h) {
			background_tex_ = nullptr;
		}
	}
	if (!background_tex_.get() && screen.get()) {
		texture_from_texture(screen, background_tex_, &rect, 0, 0);
	}
}

void ttrack::impl_draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);

	const SDL_Rect rect = get_rect();
	reset_background_texture(frame_buffer, rect);

	if (did_draw_) {
		did_draw_(*this, rect, true);
	}
}

texture ttrack::get_canvas_tex()
{
	texture result = tcontrol::get_canvas_tex();

	const SDL_Rect rect{0, 0, (int)w_, (int)h_};
	reset_background_texture(result, rect);
	if (did_draw_) {
		did_draw_(*this, rect, true);
	}

	return result;
}

SDL_Rect ttrack::get_draw_rect() const
{
	if (float_widget_) {
		return ::create_rect(0, 0, w_, h_);
	}
	return ::create_rect(x_, y_, w_, h_);
}

void ttrack::timer_handler()
{
	if (did_draw_ && background_tex_) {
		const SDL_Rect rect = get_draw_rect();
		did_draw_(*this, rect, false);
	}
}

void ttrack::set_timer_interval(int interval) 
{ 
	VALIDATE(interval >= 0, null_str);
	twindow* window = get_window();
	VALIDATE(window, "must call set_timer_interval after window valid.");

	if (timer_interval_ != interval) {
		if (timer_.valid()) {
			timer_.reset();
		}
		if (interval) {
			timer_.reset(interval, *window, boost::bind(&ttrack::timer_handler, this));
		}
		timer_interval_ = interval;
	}
}

void ttrack::clear_texture()
{
	tcontrol::clear_texture();
	if (background_tex_.get()) {
		background_tex_ = nullptr;
	}
}

const std::string& ttrack::get_control_type() const
{
	static const std::string type = "track";
	return type;
}

void ttrack::signal_handler_mouse_leave(const tpoint& coordinate)
{
	if (is_null_coordinate(first_coordinate_)) {
		return;
	}

	if (did_mouse_leave_) {
		did_mouse_leave_(*this, first_coordinate_, coordinate);
	}
	set_null_coordinate(first_coordinate_);
}

void ttrack::signal_handler_left_button_down(const tpoint& coordinate)
{
	VALIDATE(is_null_coordinate(first_coordinate_), null_str);

	twindow* window = get_window();
	if (window && require_capture_) {
		window->mouse_capture();
	}
	first_coordinate_ = coordinate;

	if (did_left_button_down_) {
		did_left_button_down_(*this, coordinate);
	}
}

void ttrack::signal_handler_mouse_motion(const tpoint& coordinate)
{
	VALIDATE(!is_mouse_leave_window_event(coordinate), null_str);

	if (did_mouse_motion_) {
		did_mouse_motion_(*this, first_coordinate_, coordinate);
	}
}

} // namespace gui2
