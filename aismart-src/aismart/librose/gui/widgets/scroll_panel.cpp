/* $Id: scroll_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/scroll_panel.hpp"

#include "gui/auxiliary/widget_definition/scroll_panel.hpp"
#include "gui/auxiliary/window_builder/scroll_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scroll_panel)

tscroll_panel::tscroll_panel(const unsigned canvas_count)
	: tscroll_container(canvas_count)
{
	find_content_grid_ = true;
}

tspace4 tscroll_panel::mini_conf_margin() const
{
	boost::intrusive_ptr<const tscroll_panel_definition::tresolution> confptr =
		boost::dynamic_pointer_cast<const tscroll_panel_definition::tresolution>(config());
	const tscroll_panel_definition::tresolution& conf = *confptr;

	return tspace4{conf.left_margin, conf.right_margin, conf.top_margin, conf.bottom_margin};
}

tpoint tscroll_panel::mini_calculate_content_grid_size(const tpoint& content_origin, const tpoint& content_size)
{
	tpoint size(0, 0);
	{
	 	tclear_restrict_width_cell_size_lock lock;
		size = content_grid_->get_best_size();
		size.x = std::max(size.x, content_size.x);
	}
	tpoint ret = content_grid_->calculate_best_size_bh(size.x);
	VALIDATE(ret.x <= size.x, null_str);
	ret.y = std::max(ret.y, content_size.y);

	return ret;
}

// amone widget derived from scroll_container, there is only scroll_panel.
tpoint tscroll_panel::calculate_best_size_bh(const int width)
{
	//
	// best_size use content_grid_->calculate_best_size_bh is always full size. widget maybe from *.cfg.
	// below code is from tcontrol::calculate_best_size.
	//
	const tpoint cfg_size = best_size_from_builder();
	if (cfg_size.x != nposm && !width_is_max_ && cfg_size.y != nposm && !height_is_max_) {
		return cfg_size;
	}

	tpoint text_size = content_grid_->calculate_best_size_bh(width);

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

const std::string& tscroll_panel::get_control_type() const
{
    static const std::string type = "scroll_panel";
    return type;
}

} // namespace gui2

