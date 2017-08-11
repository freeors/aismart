/* $Id: campaign_difficulty.hpp 49603 2011-05-22 17:56:17Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_VISUAL_STUDIO_HPP_INCLUDED
#define GUI_DIALOGS_VISUAL_STUDIO_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "gui/widgets/grid.hpp"
#include <vector>

namespace gui2 {

class tbutton;
class tlabel;
class ttrack;
class tstack;
class tgrid;

class tvisual_layout : public tdialog
{
public:
	explicit tvisual_layout(const twindow& target, const std::string& reason);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	/** Inherited from tdialog. */
	void pre_show() override;

	void did_layout_foreground(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);

	std::string generate_widget_str(const twidget& widget, const SDL_Rect& cell_rect);

	bool is_extensible(const twidget& widget) const;
	std::pair<const twidget*, SDL_Rect> which_hit(twindow& window, int x, int y) const;
	void draw_grid_cell(SDL_Renderer* renderer, const tgrid& grid, const tpoint& origin);

	void signal_handler_mouse_motion(twindow& window, const tpoint& coordinate);
	void signal_handler_left_button_up(twindow& window, const tpoint& coordinate);

private:
	const twindow& target_;
	const std::string& reason_;

	std::map<const twidget*, SDL_Rect> widgets_;
	std::map<const tstack*, SDL_Rect> statk_in_widgets_;

	std::map<const tstack*, int> stacks_;
	uint32_t stacked_color_;

	float width_factor_;
	float height_factor_;
};


}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
