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

#ifndef GUI_DIALOGS_PHOTO_SELECTOR_HPP_INCLUDED
#define GUI_DIALOGS_PHOTO_SELECTOR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

#include <vector>

namespace gui2 {

class tbutton;
class tlistbox;

struct twalk_photo
{
	twalk_photo(const std::string& dir)
		: dir(dir)
	{
		photos = SDL_OpenPhoto(dir.c_str());
	}
	~twalk_photo()
	{
		SDL_ClosePhoto(photos);
	}

	std::string dir;
	SDL_Photo* photos;
};

class tphoto_selector : public tdialog
{
public:
	explicit tphoto_selector(const std::string& title, const std::string& initial_dir);
	~tphoto_selector();

	/**
	 * Returns the selected item index after displaying.
	 * @return -1 if the dialog was cancelled.
	 */
	surface selected_surf();

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	void item_selected(twindow& window, tlistbox& list);

	void did_navigation_item_changed(ttoggle_button& widget);
	bool did_photos_item_pre_change(ttoggle_button& widget);
	void did_photos_item_changed(ttoggle_button& widget);

	void did_surf_valid(surface surf, int from);

private:
	const std::string title_;
	const std::string initial_dir_;
	std::set<int> selected_photos_;

	class texecutor: public tworker
	{
	public:
		texecutor(tphoto_selector& selector)
			: selector_(selector)
		{
			thread_->Start();
		}

	private:
		void DoWork() override;
		void OnWorkStart() override {}
		void OnWorkDone() override {}

	private:
		tphoto_selector& selector_;
	};
	std::unique_ptr<texecutor> executor_;
	bool load_thread_running_;
	bool load_thread_exited_;
	threading::mutex change_range_mutex_;

	tpoint load_range_;
	tpoint image_size_;
	std::vector<bool> images_valid_;

	surface loading_;
	surface unknown_;
	std::unique_ptr<twalk_photo> walk_photo_;
	tbutton* ok_;
};

}


#endif /* ! GUI_DIALOGS_PHOTO_SELECTOR_HPP_INCLUDED */
