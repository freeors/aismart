#define GETTEXT_DOMAIN "rose-lib"

#include "control.hpp"

#include "font.hpp"
#include "formula_string_utils.hpp"
#include "gui/auxiliary/event/message.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "marked-up_text.hpp"
#include "area_anim.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <iomanip>

namespace gui2 {

tout_of_chain_widget_lock::tout_of_chain_widget_lock(twidget& widget)
	: window_(nullptr)
{
	window_ = widget.get_window();
	window_->set_out_of_chain_widget(&widget);
}
tout_of_chain_widget_lock::~tout_of_chain_widget_lock()
{
	window_->set_out_of_chain_widget(nullptr);
}

void tfloat_widget::set_visible(bool visible)
{
	widget->set_visible(visible? twidget::VISIBLE: twidget::INVISIBLE);
}

tfloat_widget::~tfloat_widget()
{
	// when destruct, parent of widget must be window.
	// widget->set_parent(&window);
	VALIDATE(widget->parent() == &window, null_str);
}

void tfloat_widget::set_ref_widget(tcontrol* widget, const tpoint& mouse)
{
	bool dirty = false;
	if (!widget) {
		VALIDATE(is_null_coordinate(mouse), null_str);
	}
	if (widget != ref_widget_) {
		if (ref_widget_) {
			ref_widget_->associate_float_widget(*this, false);
		}
		ref_widget_ = widget;
		dirty = true;
	}
	if (mouse != mouse_) {
		mouse_ = mouse;
		dirty = true;
	}
	if (dirty) {
		need_layout = true;
	}
}

void tfloat_widget::set_ref(const std::string& ref)
{
	if (ref == ref_) {
		return;
	}
	if (!ref_.empty()) {
		tcontrol* ref_widget = dynamic_cast<tcontrol*>(window.find(ref, false));
		if (ref_widget) {
			ref_widget->associate_float_widget(*this, false);
		}
	}
	ref_ = ref;
	need_layout = true;
}

tcontrol::tcontrol(const unsigned canvas_count)
	: label_()
	, label_size_(std::make_pair(nposm, tpoint(0, 0)))
	, mtwusc_(false)
	, text_editable_(false)
	, post_anims_()
	, integrate_(NULL)
	, tooltip_()
	, canvas_(canvas_count)
	, config_(NULL)
	, text_maximum_width_(0)
	, width_is_max_(false)
	, height_is_max_(false)
	, min_text_width_(0)
	, calculate_text_box_size_(false)
	, text_font_size_(0)
	, text_color_tpl_(0)
	, best_width_1th_(nposm)
	, best_height_1th_(nposm)
	, best_width_2th_("")
	, best_height_2th_("")
	, at_(nposm)
	, gc_distance_(nposm)
	, gc_width_(nposm)
	, gc_height_(nposm)
{
	connect_signal<event::SHOW_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_show_tooltip
			, this
			, _2
			, _3
			, _5));

	connect_signal<event::NOTIFY_REMOVE_TOOLTIP>(boost::bind(
			  &tcontrol::signal_handler_notify_remove_tooltip
			, this
			, _2
			, _3));
}

tcontrol::~tcontrol()
{
	while (!post_anims_.empty()) {
		erase_animation(post_anims_.front());
	}
}

bool tcontrol::disable_click_dismiss() const
{
	return get_visible() == twidget::VISIBLE && get_active();
}

tpoint tcontrol::get_config_min_text_size() const
{
	VALIDATE(min_text_width_ >= config_->min_width, null_str);
	tpoint result(min_text_width_, config_->min_height);
	return result;
}

void tcontrol::layout_init(bool linked_group_only)
{
	// Inherited.
	twidget::layout_init(linked_group_only);
	if (!linked_group_only) {
		text_maximum_width_ = 0;
	}
}

void tcontrol::set_text_maximum_width(int maximum)
{
	if (restrict_width_) {
		text_maximum_width_ = calculate_maximum_text_width(maximum);
	}
}

int tcontrol::get_multiline_best_width() const
{
	VALIDATE(restrict_width_, null_str);
	int ret = text_maximum_width_ + config_->text_extra_width;
	if (config_->label_is_text) {
		ret += config_->text_space_width;
	}
	return ret;
}

int tcontrol::calculate_maximum_text_width(const int maximum_width) const
{
	int ret = maximum_width - config_->text_extra_width;
	if (config_->label_is_text) {
		ret -= config_->text_space_width;
	}
	return ret >=0 ? ret: 0;
}

int tcontrol::best_width_from_text_width(const int text_width) const
{
	int ret = text_width + config_->text_extra_width;
	if (text_width && config_->label_is_text) {
		ret += config_->text_space_width;
	}
	return ret;
}

int tcontrol::best_height_from_text_height(const int text_height) const
{
	int ret = text_height + config_->text_extra_height;
	if (text_height && config_->label_is_text) {
		ret += config_->text_space_height;
	}
	return ret;
}

tpoint tcontrol::get_best_text_size(const int maximum_width) const
{
	VALIDATE(!label_.empty() && maximum_width > 0, null_str);

	if (label_size_.first == nposm || maximum_width != label_size_.first) {
		// Try with the minimum wanted size.
		label_size_.first = maximum_width;

		label_size_.second = font::get_rendered_text_size(label_, maximum_width, get_text_font_size(), font::NORMAL_COLOR, text_editable_);
	}

	const tpoint& size = label_size_.second;
	VALIDATE(size.x <= maximum_width, null_str);

	return size;
}

tpoint tcontrol::best_size_from_builder() const
{
	tpoint result(nposm, nposm);
	const twindow* window = NULL;

	if (best_width_1th_ != nposm) {
		result.x = best_width_1th_;

	} else if (best_width_2th_.has_formula2()) {
		window = get_window();
		result.x = best_width_2th_(window->variables());
		if (result.x < 0) {
			result.x = 0;
		}
		if (best_width_2th_.has_multi_or_noninteger_formula()) {
			result.x *= twidget::hdpi_scale;
		} else {
			result.x = cfg_2_os_size(result.x);
		}
	}

	if (best_height_1th_ != nposm) {
		result.y = best_height_1th_;

	} else if (best_height_2th_.has_formula2()) {
		if (!window) {
			window = get_window();
		}
		result.y = best_height_2th_(window->variables());
		if (result.y < 0) {
			result.y = 0;
		}
		if (best_height_2th_.has_multi_or_noninteger_formula()) {
			result.y *= twidget::hdpi_scale;
		} else {
			result.y = cfg_2_os_size(result.y);
		}
	}
	return result;
}

tpoint tcontrol::calculate_best_size() const
{
	VALIDATE(config_, null_str);

	if (restrict_width_ && (clear_restrict_width_cell_size || !text_maximum_width_)) {
		return tpoint(0, 0);
	}

	// if has width/height field, use them. or calculate.
	const tpoint cfg_size = best_size_from_builder();
	if (cfg_size.x != nposm && !width_is_max_ && cfg_size.y != nposm && !height_is_max_) {
		return cfg_size;
	}

	if (mtwusc_) {
		VALIDATE(config_->label_is_text && cfg_size.x == nposm, null_str);
	}

	// calculate text size.
	tpoint text_size(0, 0);
	if (config_->label_is_text) {
		if ((!text_editable_ || calculate_text_box_size_) && !label_.empty()) {
			int maximum_text_width = calculate_maximum_text_width(cfg_size.x != nposm? cfg_size.x: INT32_MAX);
			
			if (text_maximum_width_ && maximum_text_width > text_maximum_width_) {
				maximum_text_width = text_maximum_width_;
			}
			text_size = get_best_text_size(maximum_text_width);
			if (text_size.x) {
				text_size.x += config_->text_space_width;
			}
			if (text_size.y) {
				text_size.y += config_->text_space_height;
			}
		}

	} else {
		VALIDATE(!text_editable_, null_str);
		text_size = mini_get_best_text_size();
	}

	// text size must >= minimum size. 
	const tpoint minimum = get_config_min_text_size();
	if (minimum.x > 0 && text_size.x < minimum.x) {
		text_size.x = minimum.x;
	}
	if (minimum.y > 0 && text_size.y < minimum.y) {
		text_size.y = minimum.y;
	}

	tpoint result(text_size.x + config_->text_extra_width, text_size.y + config_->text_extra_height);
	if (!width_is_max_) {
		if (cfg_size.x != nposm) {
			result.x = cfg_size.x;
		}
	} else {
		if (cfg_size.x != nposm && result.x >= cfg_size.x) {
			result.x = cfg_size.x;
		}
	}
	if (!height_is_max_) {
		if (cfg_size.y != nposm) {
			result.y = cfg_size.y;
		}
	} else {
		if (cfg_size.y != nposm && result.y >= cfg_size.y) {
			result.y = cfg_size.y;
		}
	}

	return result;
}

tpoint tcontrol::calculate_best_size_bh(const int width)
{
	if (!mtwusc_) {
		return tpoint(nposm, nposm);
	}

	// reference to http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=133&extra=page%3D1%26filter%3Dtypeid%26typeid%3D21
	set_text_maximum_width(width);
	tpoint size = get_best_size();
	if (size.x > config_->text_extra_width) {
		size.x = width;
	} else {
		VALIDATE(size.x == config_->text_extra_width, null_str);
		VALIDATE(label_.empty(), null_str);
	}
	return size;
}

void tcontrol::refresh_locator_anim(std::vector<tintegrate::tlocator>& locator)
{
	if (!text_editable_) {
		return;
	}
	locator_.clear();
	if (integrate_) {
		integrate_->fill_locator_rect(locator, true);
	} else {
		locator_ = locator;
	}
}

void tcontrol::set_blits(const std::vector<tformula_blit>& blits)
{ 
	blits_ = blits;
	set_dirty();
}

void tcontrol::set_blits(const tformula_blit& blit)
{
	blits_.clear();
	blits_.push_back(blit);
	set_dirty();
}

void tcontrol::place(const tpoint& origin, const tpoint& size)
{
	if (restrict_width_) {
		VALIDATE(text_maximum_width_ >= size.x - config_->text_extra_width, null_str);
	}

	SDL_Rect previous_rect = ::create_rect(x_, y_, w_, h_);

	// resize canvasses
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_width(size.x);
		canvas.set_height(size.y);
	}

	// inherited
	twidget::place(origin, size);

	// update the state of the canvas after the sizes have been set.
	update_canvas();

	if (::create_rect(x_, y_, w_, h_) != previous_rect) {
		for (std::set<tfloat_widget*>::const_iterator it = associate_float_widgets_.begin(); it != associate_float_widgets_.end(); ++ it) {
			tfloat_widget& item = **it;
			item.need_layout = true;
		}
	}
}

void tcontrol::set_definition(const std::string& definition, const std::string& min_text_width)
{
	VALIDATE(!config_, null_str);

	config_ = get_control(get_control_type(), definition);
	VALIDATE(config_, null_str);

	if (!min_text_width.empty()) {
		// now get_window() is nullptr, so use explicited variables.
		game_logic::map_formula_callable variables;
		get_screen_size_variables(variables);

		tformula<unsigned> formula(min_text_width);
		min_text_width_ = formula(variables);
		VALIDATE(min_text_width_ >= 0, null_str);

		if (formula.has_multi_or_noninteger_formula()) {
			min_text_width_ *= twidget::hdpi_scale;
		} else {
			min_text_width_ = cfg_2_os_size(min_text_width_);
		}
	}
	min_text_width_ = posix_max(config_->min_width, min_text_width_);

	VALIDATE(canvas().size() == config_->state.size(), null_str);
	for (size_t i = 0; i < canvas_.size(); ++i) {
		canvas(i) = config()->state[i].canvas;
		canvas(i).start_animation();
	}

	const std::string border = mini_default_border();
	set_border(border);
	set_icon(null_str);

	update_canvas();

	load_config_extra();
}

void tcontrol::set_best_size_1th(const int width, const int height)
{
	bool require_invalidate_layout = false;

	if (best_width_1th_ != width) {
		best_width_1th_ = width;
		require_invalidate_layout = true;
	}
	if (best_height_1th_ != height) {
		best_height_1th_ = height;
		require_invalidate_layout = true;
	}
	
	if (require_invalidate_layout) {
		trigger_invalidate_layout(get_window());
	}
}

void tcontrol::set_best_size_2th(const std::string& width, bool width_is_max, const std::string& height, bool height_is_max)
{
	if (width_is_max) {
		VALIDATE(!width.empty(), null_str);
	}
	if (height_is_max) {
		VALIDATE(!height.empty(), null_str);
	}

	best_width_2th_ = tformula<unsigned>(width);
	width_is_max_ = width_is_max;

	best_height_2th_ = tformula<unsigned>(height);
	height_is_max_ = height_is_max;
}

void tcontrol::set_mtwusc()
{
	VALIDATE(best_width_1th_ == nposm && !best_width_2th_.has_formula(), null_str);
	VALIDATE(best_height_1th_ == nposm && !best_height_2th_.has_formula(), null_str);
	VALIDATE(config_->label_is_text, null_str);
	VALIDATE(min_text_width_ == config_->min_width, null_str);

	mtwusc_ = true;
	set_restrict_width();
}

void tcontrol::set_icon(const std::string& icon) 
{
	const size_t pos = icon.rfind(".");
	if (config_->icon_is_main && pos != std::string::npos && pos + 4 == icon.size()) {
		set_canvas_variable("icon", variant(icon.substr(0, pos))); 	
	} else {
		set_canvas_variable("icon", variant(icon)); 
	}
}

void tcontrol::set_label(const std::string& label)
{
	VALIDATE(w_ >= 0 && h_ >= 0, null_str);
	VALIDATE(!text_editable_, null_str);

	if (label == label_) {
		return;
	}

	label_ = label;
	update_canvas();
	set_dirty();
	if (!config_->label_is_text) {
		VALIDATE(label_size_.first == nposm, null_str);
		return;
	}

	label_size_.first = nposm;

	if (visible_ == twidget::INVISIBLE || disalbe_invalidate_layout || float_widget_) {
		return;
	}
	twindow* window = get_window();
	if (!window || !window->layouted()) {
		// when called by tbuilder_control, window will be nullptr.
		return;
	}

	bool require_invalidate_layout = false;
	if (w_ == 0) {
		if (label_.empty()) {
			return;
		}
		require_invalidate_layout = true;
	}
	if (!require_invalidate_layout) {
		int maximum_text_width = text_maximum_width_;
		if (!maximum_text_width) {
			maximum_text_width = calculate_maximum_text_width(w_);
		}

		tpoint text_size = font::get_rendered_text_size(label, maximum_text_width, get_text_font_size(), font::NORMAL_COLOR, text_editable_);
		int width = best_width_from_text_width(text_size.x);
		int height = best_height_from_text_height(text_size.y);

		if (height != h_) {
			require_invalidate_layout = true;

		}
		if (!require_invalidate_layout) {
			if (text_maximum_width_) {
				if (width > w_) {
					require_invalidate_layout = true;
				}
			}
		}
	}

	if (require_invalidate_layout) {
		trigger_invalidate_layout(window);
	}
}

void tcontrol::set_text_editable(bool editable)
{
	if (editable == text_editable_) {
		return;
	}

	text_editable_ = editable;
	update_canvas();
	set_dirty();
}

void tcontrol::update_canvas()
{
	// set label in canvases
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable("text", variant(label_));
	}
}

void tcontrol::clear_texture()
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.clear_texture();
	}
}

bool tcontrol::exist_anim()
{
	if (!post_anims_.empty()) {
		return true;
	}
	return !canvas_.empty() && canvas_[get_state()].exist_anim();
}

int tcontrol::insert_animation(const ::config& cfg)
{
	int id = start_cycle_float_anim(cfg);
	if (id != nposm) {
		std::vector<int>& anims = post_anims_;
		anims.push_back(id);
	}
	return id;
}

void tcontrol::erase_animation(int id)
{
	bool found = false;
	std::vector<int>::iterator find = std::find(post_anims_.begin(), post_anims_.end(), id);
	if (find != post_anims_.end()) {
		post_anims_.erase(find);
		found = true;
	}
	if (found) {
		anim2::manager::instance->erase_area_anim(id);
		set_dirty();
	}
}

void tcontrol::set_canvas_variable(const std::string& name, const variant& value)
{
	BOOST_FOREACH(tcanvas& canvas, canvas_) {
		canvas.set_variable(name, value);
	}
	set_dirty();
}

int tcontrol::get_text_font_size() const
{
	int ret = text_font_size_? text_font_size_: font::SIZE_DEFAULT;
	if (ret >= font_min_cfg_size) {
		ret = font_hdpi_size_from_cfg_size(ret);
	}
	return ret;
}

void tcontrol::impl_draw_background(texture& frame_buffer, int x_offset, int y_offset)
{
	canvas(get_state()).blit(*this, frame_buffer, calculate_blitting_rectangle(x_offset, y_offset), get_dirty(), post_anims_);
}

texture tcontrol::get_canvas_tex()
{
	VALIDATE(get_dirty() || get_redraw(), null_str);

	return canvas(get_state()).get_canvas_tex(*this, post_anims_);
}

void tcontrol::associate_float_widget(tfloat_widget& item, bool associate)
{
	if (associate) {
		associate_float_widgets_.insert(&item);
	} else {
		std::set<tfloat_widget*>::iterator it = associate_float_widgets_.find(&item);
		if (it != associate_float_widgets_.end()) {
			associate_float_widgets_.erase(it);
		}
	}
}

void tcontrol::signal_handler_show_tooltip(
		  const event::tevent event
		, bool& handled
		, const tpoint& location)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	return;
#endif

	if (!tooltip_.empty()) {
		std::string tip = tooltip_;
		event::tmessage_show_tooltip message(tip, *this, location);
		handled = fire(event::MESSAGE_SHOW_TOOLTIP, *this, message);
	}
}

void tcontrol::signal_handler_notify_remove_tooltip(
		  const event::tevent event
		, bool& handled)
{
	/*
	 * This makes the class know the tip code rather intimately. An
	 * alternative is to add a message to the window to remove the tip.
	 * Might be done later.
	 */
	get_window()->remove_tooltip();
	// tip::remove();

	handled = true;
}

} // namespace gui2

