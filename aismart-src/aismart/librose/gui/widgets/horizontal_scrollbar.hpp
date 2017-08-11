/* $Id: horizontal_scrollbar.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#ifndef GUI_WIDGETS_HORIZONTAL_SCROLLBAR_HPP_INCLUDED
#define GUI_WIDGETS_HORIZONTAL_SCROLLBAR_HPP_INCLUDED

#include "gui/widgets/scrollbar.hpp"

namespace gui2 {

/** A horizontal scrollbar. */
class thorizontal_scrollbar : public tscrollbar_
{
public:

	thorizontal_scrollbar()
		: tscrollbar_(false)
	{
	}

private:
	tpoint mini_get_best_text_size() const override;

	/** Inherited from tscrollbar. */
	int get_length() const override { return get_width(); }

	/** Inherited from tscrollbar. */
	int minimum_positioner_length() const override;

	/** Inherited from tscrollbar. */
	int maximum_positioner_length() const override;

	/** Inherited from tscrollbar. */
	int offset_before() const override;

	/** Inherited from tscrollbar. */
	int offset_after() const override;

	/** Inherited from tscrollbar. */
	bool on_positioner(const tpoint& coordinate) const;

	/** Inherited from tscrollbar. */
	int on_bar(const tpoint& coordinate) const;

	/** Inherited from tscrollbar. */
	int get_length_difference(const tpoint& original, const tpoint& current) const
		{ return current.x - original.x; }

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;
};

} // namespace gui2

#endif

