/* $Id: sha1.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2007 - 2010 by ancientcc <ancientcc@leagor.com>
   Part of the Rose Project http://www.libsdl.cn/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "protobuf.hpp"

#include "base_instance.hpp"
#include <iomanip>
#include <sstream>


namespace protobuf {

void load_pb(int type)
{
	std::pair<std::string, ::google::protobuf::Message*> pair = instance->app_pblite_from_type(type);
	VALIDATE(!pair.first.empty() && pair.second, null_str);

	tfile file(pair.first, GENERIC_READ, OPEN_EXISTING);

	::google::protobuf::Message* lite = pair.second;
	int fsize = file.read_2_data();
	// file maybe invalid. if fsize = 0, will lite->clear.
	lite->ParseFromArray(file.data, fsize);
}

void write_pb(int type)
{
	std::pair<std::string, ::google::protobuf::Message*> pair = instance->app_pblite_from_type(type);
	VALIDATE(!pair.first.empty() && pair.second, null_str);

	tfile file(pair.first, GENERIC_WRITE, CREATE_ALWAYS);
	VALIDATE(file.valid(), null_str);

	::google::protobuf::Message* lite = pair.second;
	
	int size = lite->ByteSize();
	file.resize_data(size);
	lite->SerializeToArray(file.data, size);

	posix_fwrite(file.fp, file.data, size);
	file.close();
/*
	if (home_ && foreground_) {
		home_->refresh_notification_widget(nposm);
	}
*/
}

}