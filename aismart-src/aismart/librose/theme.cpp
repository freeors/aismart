/* $Id: font.cpp 47191 2010-10-24 18:10:29Z mordante $ */
/* vim:set encoding=utf-8: */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
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

#include "global.hpp"

#include "theme.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"
#include "sdl_utils.hpp"

namespace theme {

void tfields::from_cfg(const config& cfg)
{
	id = cfg["id"].str();
	app = cfg["app"].str();
	VALIDATE(!id.empty() && !app.empty(), null_str);
	
	VALIDATE(default_tpl == 0, null_str);
	for (int n = 0; n < 3; n ++) {
		text_color_tpls[default_tpl].normal = decode_color(cfg["normal_color"].str());
		text_color_tpls[default_tpl].disable = decode_color(cfg["disable_color"].str());
		text_color_tpls[default_tpl].focus = decode_color(cfg["focus_color"].str());
		text_color_tpls[default_tpl].placeholder = decode_color(cfg["placeholder_color"].str());
	}
	for (int tpl = inverse_tpl; tpl < color_tpls; tpl ++) {
		const std::string prefix = tpl == inverse_tpl? "inverse_": "title_";

		text_color_tpls[tpl].normal = decode_color(cfg[prefix + "normal_color"].str());
		if (!text_color_tpls[tpl].normal) {
			text_color_tpls[tpl].normal = text_color_tpls[default_tpl].normal;
		}
		text_color_tpls[tpl].disable = decode_color(cfg[prefix + "disable_color"].str());
		if (!text_color_tpls[tpl].disable) {
			text_color_tpls[tpl].disable = text_color_tpls[default_tpl].disable;
		}
		text_color_tpls[tpl].focus = decode_color(cfg[prefix + "focus_color"].str());
		if (!text_color_tpls[tpl].focus) {
			text_color_tpls[tpl].focus = text_color_tpls[default_tpl].focus;
		}
		text_color_tpls[tpl].placeholder = decode_color(cfg[prefix + "placeholder_color"].str());
		if (!text_color_tpls[tpl].placeholder) {
			text_color_tpls[tpl].placeholder = text_color_tpls[default_tpl].placeholder;
		}
	}

	item_focus_color = decode_color(cfg["item_focus_color"].str());
	item_highlight_color = decode_color(cfg["item_highlight_color"].str());
	menu_focus_color = decode_color(cfg["menu_focus_color"].str());
}

tfields instance;

std::string current_id;
std::string path_end_chars;

std::string color_tpl_name(int tpl)
{
	if (tpl == theme::default_tpl) {
		return _("Default color");
	} else if (tpl == theme::inverse_tpl) {
		return _("Inverse color");
	} else if (tpl == theme::title_tpl) {
		return _("Title color");
	}
	return _("Bonus color");
}

void switch_to(const config& cfg)
{
	instance.from_cfg(cfg);
	current_id = utils::generate_app_prefix_id(instance.app, instance.id);
	if (instance.app != "rose") {
		path_end_chars = game_config::generate_app_dir(instance.app) + "/images/";
	} else {
		path_end_chars.clear();
	}
}

uint32_t text_color_from_index(int tpl, int index)
{
	VALIDATE(tpl >= 0 && tpl < color_tpls, null_str);

	if (index == normal) {
		return instance.text_color_tpls[tpl].normal;
	} else if (index == disable) {
		return instance.text_color_tpls[tpl].disable;
	} else if (index == focus) {
		return instance.text_color_tpls[tpl].focus;
	} else if (index == placeholder) {
		return instance.text_color_tpls[tpl].placeholder;
	}
	
	VALIDATE(false, null_str);
	return 0;
}

}

