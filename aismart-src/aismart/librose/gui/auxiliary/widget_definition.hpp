/* $Id: widget_definition.hpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#ifndef GUI_AUXILIARY_WIDGET_DEFINITION_HPP_INCLUDED
#define GUI_AUXILIARY_WIDGET_DEFINITION_HPP_INCLUDED

#include "config.hpp"
#include "gui/auxiliary/canvas.hpp"

#include <boost/foreach.hpp>

namespace gui2 {

/**
 * Contains the state info for a resolution.
 *
 * At the moment all states are the same so there is no need to use
 * inheritance. If that is needed at some point the containers should contain
 * pointers and we should inherit from reference_counted_object.
 */
struct tstate_definition
{
	explicit tstate_definition(const config &cfg);

	tcanvas canvas;
};


/** Base class of a resolution, contains the common keys for a resolution. */
struct tresolution_definition_
	: public reference_counted_object
{
	explicit tresolution_definition_(const config& cfg);

	int window_width;
	int window_height;

	int min_width;
	int min_height;

	int text_extra_width;
	int text_extra_height;
	int text_space_width;
	int text_space_height;
	bool label_is_text;
	bool icon_is_main;

	std::vector<tstate_definition> state;
};

typedef
	boost::intrusive_ptr<tresolution_definition_>
	tresolution_definition_ptr;

typedef
	boost::intrusive_ptr<const tresolution_definition_>
	tresolution_definition_const_ptr;

/**
 * Casts a tresolution_definition_const_ptr to another type.
 *
 * @tparam T                      The type to cast to, the non const version.
 *
 * @param ptr                     The pointer to cast.
 *
 * @returns                       A reference to type casted to.
 */
template<class T>
const T& cast(tresolution_definition_const_ptr ptr)
{
	boost::intrusive_ptr<const T> conf =
			boost::dynamic_pointer_cast<const T>(ptr);
	assert(conf);
	return *conf;
}

struct tcontrol_definition: public reference_counted_object
{
	explicit tcontrol_definition(const config& cfg);

	void throw_inconsistent_label_is_text() const;

	template<class T>
	void load_resolutions(const config &cfg) {
		int at = 0;
		config::const_child_itors itors = cfg.child_range("resolution");
		BOOST_FOREACH(const config &resolution, itors) {
			resolutions.push_back(new T(resolution));
			if (at != 0) {
				if (label_is_text != resolutions.back()->label_is_text) {
					throw_inconsistent_label_is_text();
				}
			} else {
				label_is_text = resolutions.back()->label_is_text;
			}
		}
	}

	std::string id;
	std::string app;
	t_string description;

	bool label_is_text;

	std::vector<tresolution_definition_ptr> resolutions;
};

typedef boost::intrusive_ptr<tcontrol_definition> tcontrol_definition_ptr;

} // namespace gui2

#endif

