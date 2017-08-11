/* $Id: sha1.hpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010 by Benoit Timbert <benoit.timbert@free.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef LIBROSE_PROTOBUF_HPP_INCLUDED
#define LIBROSE_PROTOBUF_HPP_INCLUDED

#include <string>
#include "SDL_types.h"
#include <google/protobuf/message.h>

namespace protobuf {

void load_pb(int type);
void write_pb(int type);

}

#endif
