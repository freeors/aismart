/* $Id: text_box.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_TEXT_BOX_HPP_INCLUDED
#define GUI_WIDGETS_TEXT_BOX_HPP_INCLUDED

#include "gui/widgets/control.hpp"
#include "timer.hpp"

#include <string>
#include <boost/function.hpp>


namespace gui2 {

/** Class for a single line text area. */
class ttext_box : public tcontrol
{
public:
	ttext_box(bool multi_line, bool password, bool atom_markup);
	virtual ~ttext_box();

	virtual void set_label(const std::string& label);

	/** Inherited from tcontrol. */
	void set_active(const bool active)
		{ if(get_active() != active) set_state(active ? ENABLED : DISABLED); };

	/** Inherited from tcontrol. */
	bool get_active() const { return state_ != DISABLED; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return state_; }

	/***** ***** ***** ***** expose some functions ***** ***** ***** *****/

	void set_placeholder(const std::string& label) { placeholder_ = label; }
	void set_default_color(const uint32_t color) { integrate_default_color_ = color; }
	virtual void set_maximum_chars(const size_t maximum_chars);

	void set_password_char(const wchar_t unicode);
	void set_cipher(const bool cipher);
	bool cipher() const { return cipher_; }

	size_t get_length() { return label_.size(); }

	/***** ***** ***** setters / getters for members ***** ****** *****/

	/** Set the text_changed callback. */
	void set_did_text_changed(const boost::function<void (ttext_box& textboxt) >& did)
	{
		did_text_changed_ = did;
	}

	void set_did_extra_menu(const boost::function<void (ttext_box& widget, std::vector<tfloat_widget*>&)>& did)
	{
		did_extra_menu_ = did;
	}

	const SDL_Rect& get_selection_start() const { return selection_start_; }

	const SDL_Rect& get_selection_end() const { return selection_end_; }
	bool exist_selection() const { return selection_end_.x != nposm; }
	void clear_selection();

	const tintegrate& integrate() const { return *integrate_; }

	void insert_img(const std::string& str);
	int get_src_pos() const;
	void set_src_pos(int pos);

	/**
	 * Backspace key pressed.
	 *
	 * Unmodified                 Deletes the character before the cursor,
	 *                            ignored if at the beginning of the data.
	 * Control                    Ignored.
	 * Shift                      Ignored.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_backspace(SDL_Keymod modifier, bool& handled);

	/**
	 * Delete key pressed.
	 *
	 * Unmodified                 If there is a selection that's deleted.
	 *                            Else if not at the end of the data the
	 *                            character after the cursor is deleted.
	 *                            Else the key is ignored.
	 *                            ignored if at the beginning of the data.
	 * Control                    Ignored.
	 * Shift                      Ignored.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_delete(SDL_Keymod modifier, bool& handled);

	/**
	 * Moves the cursor to the end of all text.
	 *
	 * For a single line text this is the same as goto_end_of_line().
	 *
	 * @param select              Select the text from the original cursor
	 *                            position till the end of the data?
	 */
	void goto_end_of_data(const bool select = false);

	/**
	 * Moves the cursor to the beginning of the data.
	 *
	 * @param select              Select the text from the original cursor
	 *                            position till the beginning of the data?
	 */
	void goto_start_of_data(const bool select = false);

	/** Selects all text. */
	void select_all();

	/**
	 * Moves the cursor at the wanted position.
	 *
	 * @param offset              The wanted new cursor position.
	 * @param select              Select the text from the original cursor
	 *                            position till the new position?
	 */
	void set_cursor(const SDL_Rect& offset, const bool select);

	/**
	 * Inserts a character at the cursor.
	 *
	 * This function is preferred over set_text since it's optimized for
	 * updating the internal bookkeeping.
	 *
	 * @param unicode             The unicode value of the character to insert.
	 */
	virtual void insert_str(const std::string& str);

	/** Copies the current selection. */
	virtual void copy_selection();

	/** Pastes the current selection. */
	virtual void paste_selection();

	virtual void cut_selection();

	/***** ***** ***** setters / getters for members ***** ****** *****/

	void normalize_start_end(SDL_Rect& start, SDL_Rect& stop) const;

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void update_canvas() override;

	/**
	 * Moves the cursor to the end of the line.
	 *
	 * @param select              Select the text from the original cursor
	 *                            position till the end of the line?
	 */
	void goto_end_of_line(const bool select = false);

	/**
	 * Moves the cursor to the beginning of the line
	 *
	 * @param select              Select the text from the original cursor
	 *                            position till the beginning of the line?
	 */
	void goto_start_of_line(const bool select = false);

	/**
	 *  Deletes the character.
	 *
	 *  @param before_cursor     If true it deletes the character before the cursor
	 *                           (backspace) else the character after the cursor
	 *                           (delete).
	 */
	virtual void delete_char(const bool before_cursor);

	/** Deletes the current selection. */
	virtual void delete_selection();

	void handle_mouse_selection(const tpoint& mouse, const bool start_selection);

	void set_visible_area(const SDL_Rect& area);

	bool selectioning() const { return selectioning_; }
	bool start_selectioning() const { return start_selection_ticks_ != 0; }
	void cancel_start_selection();

	// it is called by tscroll_text_box only.
	void set_did_cursor_moved(const boost::function<void (ttext_box& textboxt) >& did)
	{
		did_cursor_moved_ = did;
	}
	int cursor_height() const;
	int text_y_offset() const { return text_y_offset_; }

	void set_password() 
	{ 
		password_ = true;
		cipher_ = true;
	}
	void set_atom_markup(const bool atom_markup) { atom_markup_ = atom_markup; }
	bool multi_line() const { return multi_line_; }

private:
	std::string mini_default_border() const override { return "textbox"; }
	bool is_text_box() const override { return true; }
	bool captured_mouse_can_to_scroll_container() const override;
	bool captured_keyboard_can_to_scroll_container() const override { return false; }

	void place(const tpoint& origin, const tpoint& size) override;
	void impl_draw_background(texture& frame_buffer, int x_offset, int y_offset) override;
	bool popup_new_window() override;

	void show_magnifier(const bool show);
	void show_edit_button(const bool show);
	void did_edit_click(twindow& window, const int);
	uint32_t calculate_cursor_color() const;

	void calculate_integrate(const int maximum_width);
	tpoint get_integrate_size() const;
	std::string to_password_str(const int chars) const;

	int get_text_maximum_width() const;
	int get_text_maximum_width2(const int w) const;
	int get_text_maximum_width3() const;

	void adjust_xpos_when_delete(const SDL_Rect& new_start);
	void cursor_timer_handler();
	bool exist_anim() override;

private:
	bool atom_markup_;

	/** Start of the selected text. */
	SDL_Rect selection_start_;

	/** Stop of the selected text. if not valid, set it's to -1. */
	SDL_Rect selection_end_;

	/** The maximum length of the text. */
	size_t maximum_length_;
	uint32_t integrate_default_color_;

	int src_pos_;

	ttimer cursor_timer_;
	bool hide_cursor_;
	Uint32 forbid_hide_ticks_;

	boost::function<void (ttext_box& widget, std::vector<tfloat_widget*>&)> did_extra_menu_;
	std::vector<tfloat_widget*> extra_menu_;

	std::unique_ptr<tout_of_chain_widget_lock> out_of_chain_widget_lock_;

private:
	/** Note the order of the states must be the same as defined in settings.hpp. */
	enum tstate { ENABLED, DISABLED, FOCUSSED, COUNT };

	void set_state(const tstate state);

	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	/**
	 * The x offset in the widget where the text starts.
	 *
	 * This value is needed to translate a location in the widget to a location
	 * in the text.
	 */
	int text_x_offset_;

	/**
	 * The y offset in the widget where the text starts.
	 *
	 * Needed to determine whether a click is on the text.
	 */
	int text_y_offset_;

	/**
	 * The height of the text itself.
	 *
	 * Needed to determine whether a click is on the text.
	 */
	int text_height_;

		/****** handling of special keys first the pure virtuals *****/

	/**
	 * Every key can have several behaviours.
	 *
	 * Unmodified                 No modifier is pressed.
	 * Control                    The control key is pressed.
	 * Shift                      The shift key is pressed.
	 * Alt                        The alt key is pressed.
	 *
	 * If modifiers together do something else as the sum of the modifiers
	 * it's listed separately eg.
	 *
	 * Control                    Moves 10 steps at the time.
	 * Shift                      Selects the text.
	 * Control + Shift            Inserts 42 in the text.
	 *
	 * There are some predefined actions for results.
	 * Unhandled                  The key/modifier is ignored and also reported
	 *                            unhandled.
	 * Ignored                    The key/modifier is ignored and it's _expected_
	 *                            the inherited classes do the same.
	 * Implementation defined     The key/modifier is ignored and it's expected
	 *                            the inherited classes will define some meaning
	 *                            to it.
	 */

	/**
	 * Up arrow key pressed.
	 *
	 * The behaviour is implementation defined.
	 */
	virtual void handle_key_up_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Down arrow key pressed.
	 *
	 * The behaviour is implementation defined.
	 */
	virtual void handle_key_down_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Left arrow key pressed.
	 *
	 * Unmodified                 Moves the cursor a character to the left.
	 * Control                    Like unmodified but a word instead of a letter
	 *                            at the time.
	 * Shift                      Selects the text while moving.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_left_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Right arrow key pressed.
	 *
	 * Unmodified                 Moves the cursor a character to the right.
	 * Control                    Like unmodified but a word instead of a letter
	 *                            at the time.
	 * Shift                      Selects the text while moving.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_right_arrow(SDL_Keymod modifier, bool& handled);

	/**
	 * Home key pressed.
	 *
	 * Unmodified                 Moves the cursor a to the beginning of the
	 *                            line.
	 * Control                    Like unmodified but to the beginning of the
	 *                            data.
	 * Shift                      Selects the text while moving.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_home(SDL_Keymod modifier, bool& handled);

	/**
	 * End key pressed.
	 *
	 * Unmodified                 Moves the cursor a to the end of the line.
	 * Control                    Like unmodified but to the end of the data.
	 * Shift                      Selects the text while moving.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_end(SDL_Keymod modifier, bool& handled);

	/**
	 * Page up key.
	 *
	 * Unmodified                 Unhandled.
	 * Control                    Ignored.
	 * Shift                      Ignored.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_page_up(SDL_Keymod /*modifier*/, bool& /*handled*/) {}

	/**
	 * Page down key.
	 *
	 * Unmodified                 Unhandled.
	 * Control                    Ignored.
	 * Shift                      Ignored.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_page_down(SDL_Keymod /*modifier*/, bool& /*handled*/) {}

private:
	std::string placeholder_;
	bool multi_line_;
	bool password_;
	bool cipher_;
	std::string password_char_;

	/** Is the mouse in selection mode, this affects selection in mouse move */
	int selection_threshold_;
	tpoint first_coordinate_;
	uint32_t start_selection_ticks_;
	bool selectioning_;

	int xpos_; // normal it is <= 0.

	/**
	 * Default key handler if none of the above functions is called.
	 *
	 * Unmodified                 If invalid unicode it's ignored.
	 *                            Else if text selected the selected text is
	 *                            replaced with the unicode character send.
	 *                            Else the unicode character is inserted after
	 *                            the cursor.
	 * Control                    Ignored.
	 * Shift                      Ignored (already in the unicode value).
	 * Alt                        Ignored.
	 */
	virtual void handle_key_default(
		bool& handled, SDL_Keycode key, SDL_Keymod modifier, Uint16 unicode);

	/**
	 * Clears the current line.
	 *
	 * Unmodified                 Clears the current line.
	 * Control                    Ignored.
	 * Shift                      Ignored.
	 * Alt                        Ignored.
	 */
	virtual void handle_key_clear_line(SDL_Keymod modifier, bool& handled);

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_mouse_motion(const event::tevent event, bool& handled, const tpoint& coordinate, const tpoint& coordinate2);

	void signal_handler_left_button_down(const event::tevent event, bool& handled, const tpoint& coordinate);

	void signal_handler_mouse_leave(const event::tevent event, bool& handled, const tpoint& coordinate);

	void signal_handler_left_button_click(const event::tevent event, bool& handled);

	void signal_handler_left_button_double_click(const event::tevent event, bool& halt);

	void signal_handler_out_of_chain(const int e, twidget* extra_widget);
	/**
	 * Text changed callback.
	 *
	 * This callback is called in key_press after the key_press event has been
	 * handled by the control. The parameters to the function are:
	 * - The widget invoking the callback
	 * - The new text of the textbox.
	 */
	boost::function<void (ttext_box&)> did_text_changed_;
	boost::function<void (ttext_box&)> did_cursor_moved_;

	/***** ***** ***** signal handlers ***** ****** *****/
	void signal_handler_sdl_key_down(const event::tevent event, bool& handled
			, const SDL_Keycode key, SDL_Keymod modifier, const Uint16 unicode);

	void signal_handler_sdl_text_input(const event::tevent event, bool& handled
			, const char* text);

	void signal_handler_receive_keyboard_focus(const event::tevent event);
	void signal_handler_lose_keyboard_focus(const event::tevent event);

	void signal_handler_right_button_click(bool& halt, const tpoint& coordinate);
};

} //namespace gui2

#endif

