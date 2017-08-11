/* $Id: control.hpp 54584 2012-07-05 18:33:49Z mordante $ */
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

#ifndef GUI_WIDGETS_CONTROL_HPP_INCLUDED
#define GUI_WIDGETS_CONTROL_HPP_INCLUDED

#include "gui/auxiliary/widget_definition.hpp"
#include "gui/auxiliary/window_builder.hpp"
#include "gui/widgets/widget.hpp"

namespace gui2 {

namespace implementation {
	struct tbuilder_control;
} // namespace implementation

class tout_of_chain_widget_lock
{
public:
	tout_of_chain_widget_lock(twidget& widget);
	~tout_of_chain_widget_lock();

private:
	twindow* window_;
};

class tfloat_widget 
{
	friend class twindow;
public:

	tfloat_widget(twindow& window, const twindow_builder::tfloat_widget& builder, tcontrol& widget)
		: window(window)
		, widget(std::unique_ptr<tcontrol>(&widget))
		, ref_(builder.ref)
		, w(builder.w)
		, h(builder.h)
		, x(builder.x)
		, y(builder.y)
		, ref_widget_(nullptr)
		, mouse_(construct_null_coordinate())
		, need_layout(true)
		, is_scrollbar(false)
		, scrollbar_size(0)
	{}
	~tfloat_widget();

	void set_visible(bool visible);
	void set_ref_widget(tcontrol* widget, const tpoint& mouse);
	tcontrol* ref_widget() const { return ref_widget_; }
	void set_ref(const std::string& ref);

	twindow& window;
	std::pair<std::string, int> w;
	std::pair<std::string, int> h;
	std::vector<std::pair<std::string, int> > x;
	std::vector<std::pair<std::string, int> > y;
	texture buf;
	texture canvas;
	std::vector<SDL_Rect> buf_rects;
	std::unique_ptr<tcontrol> widget;
	bool need_layout;
	bool is_scrollbar; // vertical/horizontal scrollbar is special, use a common varaible.
	int scrollbar_size;

private:
	tcontrol* ref_widget_; // if isn't null, ref_widget is priori to  ref.
	std::string ref_;
	tpoint mouse_;
};

/** Base class for all visible items. */
class tcontrol : public twidget
{
	friend class tscroll_container;
	friend class tlistbox;
	friend class ttree;
	friend class ttree_node;
	friend class treport;

public:
	class tcalculate_text_box_lock
	{
	public:
		tcalculate_text_box_lock(tcontrol& widget)
			: widget_(widget)
		{
			VALIDATE(!widget_.calculate_text_box_size_, null_str);
			widget_.calculate_text_box_size_ = true;
		}
		~tcalculate_text_box_lock()
		{
			widget_.calculate_text_box_size_ = false;
		}

	private:
		tcontrol& widget_;
	};

	/** @deprecated Used the second overload. */
	explicit tcontrol(const unsigned canvas_count);

	virtual ~tcontrol();

	int insert_animation(const ::config& cfg);
	void erase_animation(int id);

	void set_canvas_variable(const std::string& name, const variant& value);
	void set_restrict_width() { restrict_width_ = true; }
	void set_text_font_size(int size) { text_font_size_ = size; }
	int get_text_font_size() const;
	void set_text_color_tpl(int tpl) { text_color_tpl_ = tpl; }
	int get_text_color_tpl() const { return text_color_tpl_; }

	virtual texture get_canvas_tex();

	/** Returns the id of the state.
	 *
	 * The current state is also the index canvas_.
	 */
	virtual unsigned get_state() const = 0;

protected:
	tpoint best_size_from_builder() const;
	bool exist_anim() override;

public:

	/***** ***** ***** ***** Easy close handling ***** ***** ***** *****/

	/**
	 * Inherited from twidget.
	 *
	 * The default behavious is that a widget blocks easy close, if not it
	 * hould override this function.
	 */
	bool disable_click_dismiss() const;

	/***** ***** ***** ***** layout functions ***** ***** ***** *****/

	/**
	 * Gets the default size as defined in the config.
	 *
	 * @pre                       config_ !=  NULL
	 *
	 * @returns                   The size.
	 */
	tpoint get_config_min_text_size() const;

	/** Inherited from twidget. */
	void layout_init(bool linked_group_only) override;

	void refresh_locator_anim(std::vector<tintegrate::tlocator>& locator);

	void set_blits(const std::vector<tformula_blit>& blits);
	void set_blits(const tformula_blit& blit);
	const std::vector<tformula_blit>& get_blits() const { return blits_; }

	int at() const { return at_; }

	/** Inherited from twidget. */
	tpoint calculate_best_size() const override;
	tpoint calculate_best_size_bh(const int width) override;

	enum {float_widget_select_all, float_widget_select, float_widget_copy, float_widget_paste, float_widget_cut};
	void set_float_widget() { float_widget_ = true; }
	void associate_float_widget(tfloat_widget& item, bool associate);

protected:
	tintegrate* integrate_;
	std::vector<tintegrate::tlocator> locator_;

	/**
	 * Surface of all in state.
	 *
	 * If it is blits style button, blits_ will not empty.
	 */
	std::vector<tformula_blit> blits_;
	std::set<tfloat_widget*> associate_float_widgets_;

public:

	/** Inherited from twidget. */
	void place(const tpoint& origin, const tpoint& size) override;

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/**
	 * Uses the load function.
	 *
	 * @note This doesn't look really clean, but the final goal is refactor
	 * more code and call load_config in the ctor, removing the use case for
	 * the window. That however is a longer termine refactoring.
	 */
	// friend class twindow;

public:
	/** Inherited from twidget. */
	twidget* find_at(const tpoint& coordinate, const bool must_be_active)
	{
		return (twidget::find_at(coordinate, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}


	/** Inherited from twidget.*/
	twidget* find(const std::string& id, const bool must_be_active)
	{
		return (twidget::find(id, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}

	/** Inherited from twidget.*/
	const twidget* find(const std::string& id,
			const bool must_be_active) const
	{
		return (twidget::find(id, must_be_active)
			&& (!must_be_active || get_active())) ? this : NULL;
	}

	/**
	 * Sets the definition.
	 *
	 * This function sets the definition of a control and should be called soon
	 * after creating the object since a lot of internal functions depend on the
	 * definition.
	 *
	 * This function should be called one time only!!!
	 */
	void set_definition(const std::string& definition, const std::string& min_text_width);

	/***** ***** ***** setters / getters for members ***** ****** *****/
	const std::string& label() const { return label_; }
	virtual void set_label(const std::string& label);

	void set_border(const std::string& border) { set_canvas_variable("border", variant(border)); }
	void set_icon(const std::string& icon);

	virtual void set_text_editable(bool editable);
	bool get_text_editable() const { return text_editable_; }

	const t_string& tooltip() const { return tooltip_; }
	// Note setting the tooltip_ doesn't dirty an object.
	void set_tooltip(const t_string& tooltip)
		{ tooltip_ = tooltip; set_wants_mouse_hover(!tooltip_.empty()); }

	// const versions will be added when needed
	std::vector<tcanvas>& canvas() { return canvas_; }
	const std::vector<tcanvas>& canvas() const { return canvas_; }
	tcanvas& canvas(const unsigned index)
		{ assert(index < canvas_.size()); return canvas_[index]; }
	const tcanvas& canvas(const unsigned index) const
		{ assert(index < canvas_.size()); return canvas_[index]; }

	tresolution_definition_ptr config() { return config_; }
	tresolution_definition_const_ptr config() const { return config_; }

	void set_best_size_1th(const int width, const int height);
	void set_best_size_2th(const std::string& width, bool width_is_max, const std::string& height, bool height_is_max);
	void set_mtwusc();

	// limit width of text when calculate_best_size.
	void set_text_maximum_width(int maximum);
	int get_multiline_best_width() const;

	int calculate_maximum_text_width(const int width) const;
	int best_width_from_text_width(const int text_width) const;
	int best_height_from_text_height(const int text_height) const;

	void clear_texture() override;

protected:
	/***** ***** ***** ***** miscellaneous ***** ***** ***** *****/

	/**
	 * Updates the canvas(ses).
	 *
	 * This function should be called if either the size of the widget changes
	 * or the text on the widget changes.
	 */
	virtual void update_canvas();

protected:
	// Contain the non-editable text associated with control.
	std::string label_;

	bool mtwusc_;

	// Read only for the label?
	bool text_editable_;
	int text_font_size_;
	int text_color_tpl_;

	// When we're used as a fixed size item, this holds the best size.
	int best_width_1th_;
	int best_height_1th_;
	tformula<unsigned> best_width_2th_;
	tformula<unsigned> best_height_2th_;

	/**
	 * Contains the pointer to the configuration.
	 *
	 * Every control has a definition of how it should look, this contains a
	 * pointer to the definition. The definition is resolution dependant, where
	 * the resolution is the size of the Wesnoth application window. Depending
	 * on the resolution widgets can look different, use different fonts.
	 * Windows can use extra scrollbars use abbreviations as text etc.
	 */
	tresolution_definition_ptr config_;

	mutable std::pair<int, tpoint> label_size_;

	// some derive widtet from scroll_container, listbox, tree, report. at_ indicate the location in array.
	// of course tlistbox, tree, report require is friend class.
	int at_;

	// used for gc
	int gc_distance_;
	int gc_width_;
	int gc_height_;

	/** The maximum width for the text in a control. has git ride of text_extra_width */
	int text_maximum_width_;
	bool width_is_max_;
	bool height_is_max_;

	int min_text_width_;

	std::vector<int> post_anims_;

	/**
	 * Holds all canvas objects for a control.
	 *
	 * A control can have multiple states, which are defined in the classes
	 * inheriting from us. For every state there is a separate canvas, which is
	 * stored here. When drawing the state is determined and that canvas is
	 * drawn.
	 */
	std::vector<tcanvas> canvas_;

private:
	/**
	 * Tooltip text.
	 *
	 * The hovering event can cause a small tooltip to be shown, this is the
	 * text to be shown. At the moment the tooltip is a single line of text.
	 */
	t_string tooltip_;

	/**
	 * Load class dependant config settings.
	 *
	 * load_config will call this method after loading the config, by default it
	 * does nothing but classes can override it to implement custom behaviour.
	 */
	virtual void load_config_extra() {}

protected:
	/** Inherited from twidget. */
	void impl_draw_background(
			  texture& frame_buffer
			, int x_offset
			, int y_offset);

	/**
	 * Gets the best size for a text.
	 *
	 * @param minimum_size        The minimum size of the text.
	 * @param maximum_size        The wanted maximum size of the text, if not
	 *                            possible it's ignored. A value of 0 means
	 *                            that it's ignored as well.
	 *
	 * @returns                   The best size.
	 */
	tpoint get_best_text_size(const int maximum_width) const;

private:
	virtual std::string mini_default_border() const { return null_str; }
	virtual tpoint mini_get_best_text_size() const { return tpoint(0, 0); }

private:
	bool calculate_text_box_size_;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_show_tooltip(
			  const event::tevent event
			, bool& handled
			, const tpoint& location);

	void signal_handler_notify_remove_tooltip(
			  const event::tevent event
			, bool& handled);
};

} // namespace gui2

#endif

