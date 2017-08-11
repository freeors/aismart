/* $Id: image.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/widgets/image.hpp"

#include "../../image.hpp"
#include "gui/auxiliary/widget_definition/image.hpp"
#include "gui/auxiliary/window_builder/image.hpp"
#include "gui/widgets/settings.hpp"
#include "filesystem.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(image)

tpoint timage::mini_get_best_text_size() const
{
	tpoint text_size(0, 0);

	const std::string& name = label();
	surface image = image::get_image(image::locator(get_hdpi_name(name, twidget::hdpi_scale)));
	if (!image) {
		image = image::get_image(name);
	}

	if (image) {
		text_size = tpoint(image->w, image->h);
	}
	return text_size;
}

const std::string& timage::get_control_type() const
{
	static const std::string type = "image";
	return type;
}

} // namespace gui2

