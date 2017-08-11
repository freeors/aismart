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

#ifndef GUI_DIALOGS_GUIDE_HPP_INCLUDED
#define GUI_DIALOGS_GUIDE_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class tcontrol;
class ttrack;

class tguide_ : public tdialog
{
public:
	explicit tguide_(const std::vector<std::string>& items, int retval);

protected:
	/** Inherited from tdialog. */
	void pre_show() override;

	virtual void did_draw_image(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn, int drag_offset_x);
	virtual void set_retval(twindow& window, int retval);

	virtual void post_blit(const SDL_Rect& widget_rect, int left_sel, const SDL_Rect& left_src, const SDL_Rect& left_dst, int right_sel, const SDL_Rect& right_src, const SDL_Rect& right_dst) {}

private:
	void callback_control_drag_detect(ttrack& widget, const tpoint& first, const tpoint& last);
	void callback_set_drag_coordinate(ttrack& widget, const tpoint& first, const tpoint& last);

protected:
	std::vector<std::string> items_;
	std::vector<texture> surfs_;
	ttrack* track_;

	std::pair<tpoint, tpoint> click_coordinate_;

	bool can_exit_;
	int cursel_;
	int retval_;
};


class tguide: public tguide_
{
public:
	explicit tguide(const std::vector<std::string>& items, const std::string& startup_img, const int percent = 70);

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	void post_blit(const SDL_Rect& widget_rect, int left_sel, const SDL_Rect& left_src, const SDL_Rect& left_dst, int right_sel, const SDL_Rect& right_src, const SDL_Rect& right_dst);
	void set_retval(twindow& window, int retval);

private:
	const std::string startup_img_;
	texture startup_tex_;
	const int percent_;
	SDL_Rect startup_rect_;
};

}


#endif /* ! GUI_DIALOGS_CAMPAIGN_DIFFICULTY_HPP_INCLUDED */
