#ifndef LIBROSE_THEME_HPP_INCLUDED
#define LIBROSE_THEME_HPP_INCLUDED

#include "config.hpp"

namespace theme {

enum {normal, disable, focus, placeholder, tpl_colors, item_focus = 100, item_highlight, menu_focus};
enum {default_tpl, inverse_tpl, title_tpl, color_tpls};

struct ttext_color_tpl {
	uint32_t normal;
	uint32_t disable;
	uint32_t focus;
	uint32_t placeholder;
};

struct tfields 
{
	tfields(const std::string& id = "", const std::string& app = "")
		: id(id)
		, app(app)
	{
		memset(text_color_tpls, 0, sizeof(text_color_tpls));
		item_focus_color = 0;
		item_highlight_color = 0;
		menu_focus_color = 0;
	}
	void from_cfg(const config& cfg);

	bool operator==(const tfields& that) const
	{
		if (id != that.id) return false;
		if (app != that.app) return false;
		if (memcmp(text_color_tpls, that.text_color_tpls, sizeof(text_color_tpls))) return false;
		if (item_focus_color != that.item_focus_color) return false;
		if (item_highlight_color != that.item_highlight_color) return false;
		if (menu_focus_color != that.menu_focus_color) return false;
		return true;
	}
	bool operator!=(const tfields& that) const { return !operator==(that); }

	std::string id;
	std::string app;
	ttext_color_tpl text_color_tpls[color_tpls];
	uint32_t item_focus_color;
	uint32_t item_highlight_color;
	uint32_t menu_focus_color;
};

extern tfields instance;
extern std::string current_id;
extern std::string path_end_chars;

void switch_to(const config& cfg);
uint32_t text_color_from_index(int tpl, int index);
std::string color_tpl_name(int tpl);
}

#endif
