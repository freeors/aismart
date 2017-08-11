/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/photo_selector.hpp"

#include "gettext.hpp"
#include "filesystem.hpp"
#include "base_instance.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"

#include "thread.hpp"
#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(rose, photo_selector)

tphoto_selector::tphoto_selector(const std::string& title, const std::string& initial_dir)
	: title_(title)
	, initial_dir_(initial_dir)
	, load_thread_running_(false)
	, load_thread_exited_(true)
	, load_range_(nposm, nposm)
	, image_size_(nposm, nposm)
{
	loading_ = font::get_rendered_text(_("Loading..."), 0, font::SIZE_LARGE);
	unknown_ = font::get_rendered_text(_("Unknown"), 0, font::SIZE_LARGE);
}

tphoto_selector::~tphoto_selector()
{
	walk_photo_.reset();
}

void tphoto_selector::pre_show()
{
	window_->set_label("misc/white-background.png");

	tlabel* title = find_widget<tlabel>(window_, "title", false, true);
	title->set_label(title_);

	ok_ = find_widget<tbutton>(window_, "ok", false, true);
	ok_->set_visible(twidget::HIDDEN);

	walk_photo_.reset(new twalk_photo(initial_dir_));
	if (!walk_photo_->photos) {
		return;
	}
	
	const int photo_count = walk_photo_->photos->count;
	images_valid_.resize(photo_count, false);

	std::stringstream ss;
	const int max_photos_per_section = 30;
	treport& navigation = find_widget<treport>(window_, "navigation", false);
	for (int from = 0; from < photo_count; from += max_photos_per_section) {
		int to = from + max_photos_per_section;
		if (to > photo_count) {
			to = photo_count;
		}
		ss.str("");
		ss << (from + 1) << " -- " << to;
		tcontrol& widget = navigation.insert_item(null_str, ss.str());
		widget.set_cookie(posix_mku32(from, to - 1));
	}
	navigation.set_did_item_changed(boost::bind(&tphoto_selector::did_navigation_item_changed, this, _2));
	navigation.select_item(0);

	treport& photos = find_widget<treport>(window_, "photos", false);
	photos.set_did_item_pre_change(boost::bind(&tphoto_selector::did_photos_item_pre_change, this, _2));
	photos.set_did_item_changed(boost::bind(&tphoto_selector::did_photos_item_changed, this, _2));

	load_thread_running_ = true;
	image_size_ = photos.get_unit_size();
	executor_.reset(new texecutor(*this));
}

void tphoto_selector::post_show()
{
	load_thread_running_ = false;
	while (!load_thread_exited_) {
		// why use load_thread_exited_?
		// 1. main thread set load_thread_running_ true, and wait, wait, wait work thread exit.
		// 2. at same time, work thread complete SDL_ReadPhoto, and want to call Invoke, 
		// but main thread wait, result to system deadlock.
		// ----principle reason: work thread use a Invoke.
		instance->pump();
	}
	executor_.reset();
}

surface tphoto_selector::selected_surf()
{
	VALIDATE(selected_photos_.size() == 1, null_str);

	surface surf;
	void* ret = SDL_ReadPhoto(walk_photo_->photos, *selected_photos_.begin(), nposm, nposm);
	if (!walk_photo_->photos->read_get_surface) {
		const char* file = (const char*)ret;
		// surf = image::get_image(file);
		// screenshot's png maybe very large, in to save memory, don't use cache.
		surf = image::locator(file).load_from_disk();

	} else {
		surf = (SDL_Surface*)ret;
		// iOS returned SDL_PIXELFORMAT_ABGR8888.
	}
	VALIDATE(surf && surf->format->BytesPerPixel >= 3, null_str);

	// 1242x2208(iphone 6plus), 750x1334(iphone 6)
	const int max_size = 1334;
	if (surf->w > max_size || surf->h > max_size) {
		tpoint ratio_size = calculate_adaption_ratio_size(max_size, max_size, surf->w, surf->h);
		surf = scale_surface(surf, ratio_size.x, ratio_size.y);
	}
	surf = makesure_neutral_surface(surf);

	return surf;
}

void tphoto_selector::did_navigation_item_changed(ttoggle_button& widget)
{
	uint32_t cookie = widget.cookie();
	const int from = posix_lo16(cookie);
	int to = posix_hi16(cookie);
	
	threading::lock lock(change_range_mutex_);

	// surface surf = image::get_image("misc/loading.png");
	surface surf = loading_;
	treport& photos = find_widget<treport>(window_, "photos", false);
	ttoggle_button* cursel = photos.cursel();
	photos.clear();
	load_range_ = tpoint(from, to);

	for (int at = from; at <= to; at ++) {
		ttoggle_button& widget = *dynamic_cast<ttoggle_button*>(&photos.insert_item(null_str, null_str));
		widget.set_cookie(at);

		widget.set_blits(gui2::tformula_blit(surf, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER));
	}

	VALIDATE(selected_photos_.size() <= 1, null_str);

	for (std::set<int>::const_iterator it = selected_photos_.begin(); it != selected_photos_.end(); ++ it) {
		int at = *it;
		if (at >= from && at <= to) {
			photos.select_item(at - from);
		}
	}
}

bool tphoto_selector::did_photos_item_pre_change(ttoggle_button& widget)
{
	return images_valid_[widget.cookie()];
}

void tphoto_selector::did_photos_item_changed(ttoggle_button& widget)
{
	VALIDATE(images_valid_[widget.cookie()], null_str);

	ok_->set_visible(twidget::VISIBLE);

	selected_photos_.clear();
	selected_photos_.insert(widget.cookie());
}

void tphoto_selector::texecutor::DoWork()
{
	SDL_Photo* photos = selector_.walk_photo_->photos;
	selector_.load_thread_exited_ = false;
	while (selector_.load_thread_running_) {
		int from;
		{
			threading::lock lock(selector_.change_range_mutex_);
			from = selector_.load_range_.x;
		}
		if (from == nposm) {
			SDL_Delay(100);
			continue;
		}

		surface surf;
		void* ret = SDL_ReadPhoto(photos, from, selector_.image_size_.x, selector_.image_size_.y);
		VALIDATE(ret, null_str);

		if (!photos->read_get_surface) {
			const char* file = (const char*)ret;
			// surf = image::get_image(file);
			// screenshot's png maybe very large, in to save memory, don't use cache.
			surf = image::locator(file).load_from_disk();
			if (surf.get()) {
				// *.png maybe not a png format file.
				const int BytesPerPixel = surf->format->BytesPerPixel;
				if (surf->w >= selector_.image_size_.x * 2 && surf->h >= selector_.image_size_.y * 2 && BytesPerPixel >= 3) {
					tpoint ratio_size = calculate_adaption_ratio_size(selector_.image_size_.x, selector_.image_size_.y, surf->w, surf->h);
					surf = scale_surface(surf, ratio_size.x, ratio_size.y);
				}
			}

		} else {
			surf = (SDL_Surface*)ret;
		}

		main_->Invoke<void>(RTC_FROM_HERE, rtc::Bind(&tphoto_selector::did_surf_valid, &selector_, surf, from));
	}
	selector_.load_thread_exited_ = true;
}

void tphoto_selector::did_surf_valid(surface surf, int from)
{
	treport& photos = find_widget<treport>(window_, "photos", false);
	tcontrol& first_widget = photos.item(0);

	if (from != load_range_.x) {
		return;
	}

	if (surf && surf->format->BytesPerPixel >= 3) {
		images_valid_[from] = true;
	} else {
		surf = unknown_;
	}
	from -= first_widget.cookie();
	tcontrol& widget = photos.item(from);
	widget.set_blits(gui2::tformula_blit(surf, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER));

	load_range_.x ++;
	if (load_range_.x > load_range_.y) {
		load_range_ = tpoint(nposm, nposm);
	}
}

}
