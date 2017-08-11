/* $Id: helper.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/auxiliary/window_builder/helper.hpp"

#include "config.hpp"
#include "gui/widgets/grid.hpp"
#include "gui/widgets/window.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

namespace implementation {

unsigned get_v_align(const std::string& v_align)
{
	if (v_align == "top") {
		return tgrid::VERTICAL_ALIGN_TOP;

	} else if (v_align == "bottom") {
		return tgrid::VERTICAL_ALIGN_BOTTOM;

	} else {
		VALIDATE(v_align.empty() || v_align == "center", null_str);
		return tgrid::VERTICAL_ALIGN_CENTER;
	}
}

unsigned get_h_align(const std::string& h_align)
{
	if (h_align == "left") {
		return tgrid::HORIZONTAL_ALIGN_LEFT;

	} else if (h_align == "right") {
		return tgrid::HORIZONTAL_ALIGN_RIGHT;

	} else {
		VALIDATE(h_align.empty() || h_align == "center", null_str);
		return tgrid::HORIZONTAL_ALIGN_CENTER;
	}
}

unsigned get_v_align4(const std::string& v_align)
{
	if (v_align == "edge") {
		return tgrid::VERTICAL_ALIGN_EDGE;
	}
	return get_v_align(v_align);
}

unsigned get_h_align4(const std::string& h_align)
{
	if (h_align == "edge") {
		return tgrid::HORIZONTAL_ALIGN_EDGE;
	}
	return get_h_align(h_align);
}

twidget::torientation get_orientation(const std::string& orientation)
{
	if (orientation == "landscape") {
		return twidget::landscape_orientation;

	} else if (orientation == "portrait") {
		return twidget::portrait_orientation;

	} else {
		// auto
		return twidget::auto_orientation;
	}
}

unsigned get_border(const std::vector<std::string>& border)
{
	unsigned result = 0;
	BOOST_FOREACH(const std::string& s, border) {
		if (s == "all") {
			return tgrid::BORDER_ALL;
		} else if (s == "top") {
			result |= tgrid::BORDER_TOP;
		} else if (s == "bottom") {
			result |= tgrid::BORDER_BOTTOM;
		} else if (s == "left") {
			result |= tgrid::BORDER_LEFT;
		} else if (s == "right") {
			result |= tgrid::BORDER_RIGHT;
		}
	}
	return result;
}

unsigned read_flags(const config& cfg)
{
	unsigned flags = get_v_align4(cfg["vertical_alignment"]);
	flags |= get_h_align4(cfg["horizontal_alignment"]);

	flags |= get_border(utils::split(cfg["border"]));

	return flags;
}

tscroll_container::tscrollbar_mode
		get_scrollbar_mode(const std::string& scrollbar_mode)
{
	if (scrollbar_mode == "never") {
		return tscroll_container::always_invisible;
	} else {
		if (!scrollbar_mode.empty() && scrollbar_mode != "auto") {
			// Invalid scrollbar mode 'scrollbar_mode' falling back to 'auto'.;
		}
		return tscroll_container::auto_visible;
	}
}

tstack::tmode get_stack_mode(const std::string& mode)
{
	if (!mode.empty()) {
		if (mode == "radio") {
			return tstack::radio;
		} else if (mode == "vertical") {
			return tstack::vertical;
		} else if (mode == "horizontal") {
			return tstack::horizontal;
		}
		VALIDATE(mode == "pip", null_str);
	}
	return tstack::pip;

}

std::string form_stack_mode_str(tstack::tmode mode)
{
	if (mode == tstack::radio) {
		return "radio";
	} else if (mode == tstack::vertical) {
		return "vertical";
	} else if (mode == tstack::horizontal) {
		return "horizontal";
	}

	VALIDATE(mode == tstack::pip, null_str);
	return "pip";
}

int get_retval(const std::string& retval_id
		, const int retval
		, const std::string& id)
{
	if(!retval_id.empty()) {
		int result = twindow::get_retval_by_id(retval_id);
		if(result) {
			return result;
		} else {
			// Window builder: retval_id 'retval_id' is unknown.;
		}
	}

	if(retval) {
		return retval;
	} else {
		return twindow::get_retval_by_id(id);
	}
}

} // namespace implementation

} // namespace gui2

