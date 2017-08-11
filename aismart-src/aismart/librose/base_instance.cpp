/* $Id: mkwin_display.cpp 47082 2010-10-18 00:44:43Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2010 by Tomasz Sniatowski <kailoran@gmail.com>
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

#include "base_instance.hpp"
#include "gettext.hpp"
#include "builder.hpp"
#include "language.hpp"
#include "preferences_display.hpp"
#include "loadscreen.hpp"
#include "cursor.hpp"
#include "map.hpp"
#include "version.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/language_selection.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/widgets/window.hpp"
#include "ble.hpp"
#include "theme.hpp"

#ifdef WEBRTC_ANDROID
#include "modules/utility/include/jvm_android.h"
#include <base/android/jni_android.h>
#include <base/android/base_jni_onload.h>
#endif

// chromium
#include <base/command_line.h>

#include <iostream>
#include <clocale>
#include <boost/bind.hpp>

base_instance* instance = nullptr;

void base_instance::regenerate_heros(hero_map& heros, bool allow_empty)
{
	const std::string hero_data_path = game_config::path + "/xwml/" + "hero.dat";
	heros.map_from_file(hero_data_path);
	if (!heros.map_from_file(hero_data_path)) {
		if (allow_empty) {
			// allow no hero.dat
			heros.realloc_hero_map(HEROS_MAX_HEROS);
		} else {
			std::stringstream err;
			err << _("Can not find valid hero.dat in <data>/xwml");
			throw game::error(err.str());
		}
	}
	hero_map::map_size_from_dat = heros.size();
	hero player_hero(hero_map::map_size_from_dat);
	if (!preferences::get_hero(player_hero, heros.size())) {
		// write [hero] to preferences
		preferences::set_hero(heros, player_hero);
	}

	group.reset();
	heros.add(player_hero);
	hero& leader = heros[heros.size() - 1];
	group.set_leader(leader);
	leader.set_uid(preferences::uid());
	group.set_noble(preferences::noble());
	group.set_coin(preferences::coin());
	group.set_score(preferences::score());
	group.interior_from_str(preferences::interior());
	group.signin_from_str(preferences::signin());
	group.reload_heros_from_string(heros, preferences::member(), preferences::exile());
	group.associate_from_str(preferences::associate());
	// group.set_layout(preferences::layout()); ?????
	group.set_map(preferences::map());

	group.set_city(heros[hero::number_local_player_city]);
	group.city().set_name(preferences::city());

	other_group.clear();
}

extern void preprocess_res_explorer();
static void handle_app_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_app_event(type);
}

static void handle_window_event(Uint32 type, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_window_event(type);
}

static void handle_background(SDL_bool screen_on, void* param)
{
	base_instance* instance = reinterpret_cast<base_instance*>(param);
	instance->handle_background(screen_on? true: false);
}

bool is_all_ascii(const std::string& str)
{
	// str maybe ansi, unicode, utf-8.
	const char* c_str = str.c_str();
	int size = str.size();
	VALIDATE(size, null_str);

	for (int at = 0; at < size; at ++) {
		if (c_str[at] & 0x80) {
			return false;
		}
	}
	return true;
}

void path_must_all_ascii(const std::string& path)
{
	// path is utf-8 format.
	VALIDATE(!path.empty(), null_str);

	bool ret = is_all_ascii(path);
	if (!ret) {
		std::string path2 = utils::normalize_path(path);
#ifdef _WIN32
		conv_ansi_utf8(path2, false);
#endif
		std::stringstream err;
		err << path2 << " contains illegal characters. Please use a pure english path.";
		VALIDATE(false, err.str()); 
	}
}

base_instance::base_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv, int sample_rate, size_t sound_buffer_size)
	: icon_()
	, chromium_env_(base::test::ScopedTaskEnvironment::MainThreadType::IO)
	, video_()
	, font_manager_() 
	, prefs_manager_()
	, image_manager_()
	, music_thinker_()
	, paths_manager_()
	, anim2_manager_(video_)
	, cursor_manager_(NULL)
	, loadscreen_manager_(NULL)
	, gui2_event_manager_(NULL)
	, heros_(game_config::path)
	, app_cfg_()
	, old_defines_map_()
	, cache_(config_cache::instance())
	, foreground_(true) // normally app cannnot receive first DIDFOREGROUND.
	, terminating_(false)
	, minimized_(false)
	, background_persist_xmit_audio_(NULL)
	, background_callback_id_(INVALID_UINT32_ID)
	, rtc_client_(nullptr)
	, sdl_thread_(&ss)
{
	// base::CommandLine::InitUsingArgvForTesting(argc, argv);
	base::CommandLine::Init(argc, argv);

	void* v1 = NULL;
	void* v2 = NULL;
	SDL_GetPlatformVariable(&v1, &v2, NULL);
#ifdef WEBRTC_ANDROID
	webrtc::JVM::Initialize(reinterpret_cast<JavaVM*>(v1), reinterpret_cast<jobject>(v2));
	base::android::InitVM(reinterpret_cast<JavaVM*>(v1));
	base::android::OnJNIOnLoadInit();
#endif
	rtc::ThreadManager::Instance()->SetCurrentThread(&sdl_thread_);

	VALIDATE(game_config::path.empty(), null_str);
#ifdef _WIN32
	std::string exe_dir = get_exe_dir();
	if (!exe_dir.empty() && file_exists(exe_dir + "/data/_main.cfg")) {
		game_config::path = exe_dir;
		path_must_all_ascii(game_config::path);
	}

	// windows is debug os. allow pass command line.
	for (int arg_ = 1; arg_ != argc; ++ arg_) {
		const std::string option(argv[arg_]);
		if (option.empty()) {
			continue;
		}

		if (option == "--res-dir") {
			const std::string val = argv[++ arg_];

			// Overriding data directory
			if (val.c_str()[1] == ':') {
				game_config::path = val;
			} else {
				game_config::path = get_cwd() + '/' + val;
			}
			path_must_all_ascii(game_config::path);

			if (!is_directory(game_config::path)) {
				std::stringstream err;
				err << "Use --res-dir set " << utils::normalize_path(game_config::path) << " to game_config::path, but it isn't valid directory.";
				VALIDATE(false, err.str());
			}
		}
	}

#elif defined(__APPLE__)
	// ios, macosx
	game_config::path = get_cwd();

#elif defined(ANDROID)
	// on android, use asset.
	game_config::path = "res";

#else
	// linux, etc
	game_config::path = get_cwd();
#endif

	game_config::path = utils::normalize_path(game_config::path);
/*
	std::string path1 = utils::normalize_path(game_config::path + "//.");
	std::string path2 = utils::normalize_path(game_config::path + "//.g");
	std::string path3 = utils::normalize_path(game_config::path + "//.g/h./..");
	std::string path4 = utils::normalize_path(game_config::path + "//.g/.");
	std::string path5 = utils::normalize_path(game_config::path + "//.g/./");
	std::string path6 = utils::normalize_path(game_config::path + "//.g/.h");
*/
	VALIDATE(game_config::path.find('\\') == std::string::npos, null_str);
	VALIDATE(file_exists(game_config::path + "/data/_main.cfg"), "game_config::path(" + game_config::path + ") must be valid!");

	SDL_Log("Data directory: %s\n", game_config::path.c_str());
	preprocess_res_explorer();

	sound::set_play_params(sample_rate, sound_buffer_size);

	bool no_music = false;
	bool no_sound = false;

	// if allocate static, iOS may be not align 4! 
	// it necessary to ensure align great equal than 4.
	game_config::savegame_cache = (unsigned char*)malloc(game_config::savegame_cache_size);

	font_manager_.update_font_path();
	heros_.set_path(game_config::path);

#if defined(_WIN32) && defined(_DEBUG)
	// sound::init_sound make no memory leak output.
	// By this time, I doesn't find what result it, to easy, don't call sound::init_sound. 
	no_sound = true;
#endif

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {

		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	} else if (no_music) { // else disable the music in nomusic mode
		preferences::set_music(false);
	}

	game_config::reserve_players.insert("");
	game_config::reserve_players.insert("kingdom");
	game_config::reserve_players.insert("Player");
	game_config::reserve_players.insert(_("Player"));


	//
	// initialize player group
	//
	upgrade::fill_require();
	regenerate_heros(heros_, true);

	SDL_SetAppEventHandler(::handle_app_event, this);
	SDL_SetWindowEventHandler(::handle_window_event, this);
	SDL_SetBackgroundHandler(::handle_background, this);
}

base_instance::~base_instance()
{
	close();

	if (icon_) {
		icon_.get()->refcount --;
	}
	if (game_config::savegame_cache) {
		free(game_config::savegame_cache);
		game_config::savegame_cache = NULL;
	}
	terrain_builder::release_heap();
	sound::close_sound();

	clear_anims();
	game_config::path.clear();

	base::CommandLine::Reset();
}

/**
 * I would prefer to setup locale first so that early error
 * messages can get localized, but we need the game_controller
 * initialized to have get_intl_dir() to work.  Note: setlocale()
 * does not take GUI language setting into account.
 */
void base_instance::init_locale() 
{
#ifdef _WIN32
    std::setlocale(LC_ALL, "English");
#else
	std::setlocale(LC_ALL, "C");
	std::setlocale(LC_MESSAGES, "");
#endif
	const std::string& intl_dir = get_intl_dir();
	bindtextdomain("rose-lib", intl_dir.c_str());
	bind_textdomain_codeset ("rose-lib", "UTF-8");
	
	bindtextdomain(def_textdomain, intl_dir.c_str());
	bind_textdomain_codeset(def_textdomain, "UTF-8");
	textdomain(def_textdomain);

	app_init_locale(intl_dir);
}

bool base_instance::init_language()
{
	if (!::load_language_list()) {
		return false;
	}

	if (!::set_language(get_locale())) {
		return false;
	}

	// hotkey::load_descriptions();

	return true;
}

uint32_t base_instance::get_callback_id() const
{
	uint32_t id = background_callback_id_ + 1;
	while (true) {
		if (id == INVALID_UINT32_ID) {
			// turn aournd.
			id ++;
			continue;
		}
		if (background_callbacks_.count(id)) {
			id ++;
			continue;
		}
		break;
	}
	return id;
}

uint32_t base_instance::background_connect(const boost::function<bool (uint32_t ticks, bool screen_on)>& callback)
{
	SDL_Log("base_instance::background_connect------, callbacks_: %u\n", background_callbacks_.size());
	if (!foreground_) {
		if (background_callbacks_.empty()) {
			VALIDATE(!background_persist_xmit_audio_.get(), null_str);
			background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
        VALIDATE(background_persist_xmit_audio_.get(), null_str);
	}

	background_callback_id_ = get_callback_id();
	background_callbacks_.insert(std::make_pair(background_callback_id_, callback));

	SDL_Log("------base_instance::background_connect, callbacks_: %u\n", background_callbacks_.size());
	return background_callback_id_;
}

void base_instance::background_disconnect_th(const uint32_t id)
{
	if (!foreground_) {
		VALIDATE(background_persist_xmit_audio_.get(), null_str);
		if (background_callbacks_.size() == 1) {
			background_persist_xmit_audio_.reset(NULL);
		}
	}
}

void base_instance::background_disconnect(const uint32_t id)
{
	SDL_Log("base_instance::background_disconnect------, callbacks_: %u\n", background_callbacks_.size());

	background_disconnect_th(id);

	VALIDATE(background_callbacks_.count(id), null_str);
	std::map<uint32_t, boost::function<bool (uint32_t, bool)> >::iterator it = background_callbacks_.find(id);
	background_callbacks_.erase(it);

	SDL_Log("------base_instance::background_disconnect, callbacks_: %u\n", background_callbacks_.size());
}

void base_instance::handle_background(bool screen_on)
{
	uint32_t ticks = SDL_GetTicks();
	for (std::map<uint32_t, boost::function<bool (uint32_t ticks, bool screen_on)> >::iterator it = background_callbacks_.begin(); it != background_callbacks_.end(); ) {
		// must not app call background_disconnect! once enable, this for will result confuse!
		bool erase = it->second(ticks, screen_on);
		if (erase) {
			background_disconnect_th(it->first);
			background_callbacks_.erase(it ++);
		} else {
			++ it;
		}
	}
}

void base_instance::handle_app_event(Uint32 type)
{
	// Notice! it isn't in main thread, except SDL_APP_LOWMEMORY.
	if (type == SDL_APP_TERMINATING || type == SDL_QUIT) {
		SDL_Log("handle_app_event, SDL_APP_TERMINATING(0x%x)\n", type);
		app_terminating();

		terminating_ = true;
#ifdef ANDROID
		// now android is default!

#elif defined __APPLE__
		// now iOS is default!
		// if call CVideo::quit when SDL_APP_WILLENTERBACKGROUND, it will success. Of couse, no help.
		throw CVideo::quit();
#endif

	} else if (type == SDL_APP_WILLENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERBACKGROUND\n");
		app_willenterbackground();
		// FIX SDL BUG! normally DIDENTERBACKGROUND should be called after WILLENTERBACKGROUND.
		// but on iOS, because SDL event queue, SDL-DIDENTERBACKGROUND is called, but app-DIDENTERBACKGROUND not!
		// app-DIDENTERBACKGROUND is call when WILLENTERFOREGROUND.

	} else if (type == SDL_APP_DIDENTERBACKGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERBACKGROUND\n");
		app_didenterbackground();

		foreground_ = false;
		
		// app require insert callback in app_didenterbackground.
		VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		if (!background_callbacks_.empty()) {
			background_persist_xmit_audio_.reset(new sound::tpersist_xmit_audio_lock);
		}
		tble* ble = tble::get_singleton();
		if (ble) {
			ble->handle_app_event(true);
		}

	} else if (type == SDL_APP_WILLENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_WILLENTERFOREGROUND\n");
		app_willenterforeground();

	} else if (type == SDL_APP_DIDENTERFOREGROUND) {
		SDL_Log("handle_app_event, SDL_APP_DIDENTERFOREGROUND\n");
		// force clear all backgroud callback. app requrie clear callback_id in app_didenterforeground.
		if (!background_callbacks_.empty()) {
			VALIDATE(background_persist_xmit_audio_.get() != NULL, null_str);
            background_persist_xmit_audio_.reset(NULL);
			background_callbacks_.clear();
		} else {
			VALIDATE(background_persist_xmit_audio_.get() == NULL, null_str);
		}
		app_didenterforeground();
	
		foreground_ = true;
		tble* ble = tble::get_singleton();
		if (ble) {
			ble->handle_app_event(false);
		}

	} else if (type == SDL_APP_LOWMEMORY) {
		SDL_Log("handle_app_event, SDL_APP_LOWMEMORY\n");
		app_lowmemory();
	}
}

void base_instance::handle_window_event(Uint32 type)
{
	if (type == SDL_WINDOWEVENT_MINIMIZED) {
		minimized_ = true;
		SDL_Log("handle_window_event, MINIMIZED\n");

	} else if (type == SDL_WINDOWEVENT_MAXIMIZED || type == SDL_WINDOWEVENT_RESTORED) {
		minimized_ = false;
		SDL_Log("handle_window_event, MAXIMIZED or RESTORED\n");

	}
}

surface icon2;
bool base_instance::init_video()
{
	// So far not initialize font, don't call function related to the font. 
	// for example get_rendered_text/get_rendered_text_size.
	std::pair<int,int> resolution;
	int video_flags = 0;

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	// width/height from preferences.cfg must be landscape.
    SDL_Rect rc = video_.bound();

	SDL_Log("1. video_.bound(): (%i, %i, %i, %i)\n", rc.x, rc.y, rc.w, rc.h);

	if (rc.w < rc.h) {
		int tmp = rc.w;
		rc.w = rc.h;
		rc.h = tmp;
	}

#if (defined(__APPLE__) && TARGET_OS_IPHONE)
	rc.w *= gui2::twidget::hdpi_scale;
	rc.h *= gui2::twidget::hdpi_scale;
#endif

	if (!gui2::twidget::should_conside_orientation(rc.w, rc.h)) {
		// for iOS/Android, it is get screen's width/height first.
		// rule: if should_conside_orientation() is false, current_landscape always must be true.
		// gui2::twidget::current_landscape = true;
	}
	tpoint normal_size = gui2::twidget::orientation_swap_size(rc.w, rc.h);

	resolution = std::make_pair(normal_size.x, normal_size.y);
	// on iphone/ipad, don't set SDL_WINDOW_FULLSCREEN, it will result cannot find orientation.
	// video_flags = SDL_WINDOW_FULLSCREEN;

#else
	video_flags = preferences::fullscreen() ? SDL_WINDOW_FULLSCREEN: 0;
	resolution = preferences::resolution(video_);
#endif

	SDL_Log("setting mode to %ix%ix32\n", resolution.first, resolution.second);
	if (!video_.setMode(resolution.first, resolution.second, video_flags)) {
		std::cerr << "required video mode, " << resolution.first << "x" << resolution.second << "x32" << " is not supported\n";
		return false;
	}
	SDL_Log("using mode %ix%ix32\n", video_.getx(), video_.gety());

	{
		// render startup image. 
		// normally, app use this png only once, and it' size is large. to save memory, don't add to cache.
		surface surf = image::locator("assets/Default-414w-736h@3x.png").load_from_disk();
		if (!surf) {
			return false;
		}
		surf = makesure_neutral_surface(surf);

		tpoint ratio_size = calculate_adaption_ratio_size(video_.getx(), video_.gety(), surf->w, surf->h);
		surface bg = create_neutral_surface(video_.getx(), video_.gety());
		if (ratio_size.x != video_.getx() || ratio_size.y != video_.gety()) {
			fill_surface(bg, 0xffffffff);
		}
		surf = scale_surface(surf, ratio_size.x, ratio_size.y);
		SDL_Rect dstrect {(video_.getx() - ratio_size.x) / 2, (video_.gety() - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
		sdl_blit(surf, nullptr, bg, &dstrect);
		render_surface(get_renderer(), bg, nullptr, nullptr);
		video_.flip();
	}

	{
		cache_.clear_defines();
		config cfg;
		cache_.get_config(game_config::path + "/data/settings.cfg", cfg);
		const config& rose_settings_cfg = cfg.child("settings");
		VALIDATE(!rose_settings_cfg.empty(), null_str);
		game_config::load_config(&rose_settings_cfg);

		cache_.clear_defines();
		cache_.get_config(game_config::path + "/data/" + game_config::generate_app_dir(game_config::app) + "/settings.cfg", cfg);
		const config& app_settings_cfg = cfg.child("settings");
		app_load_settings_config(app_settings_cfg? app_settings_cfg: config());
	}

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::wesnoth_version.str();
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());

#ifdef _WIN32
	icon2 = image::get_image("game-icon.png");
	if (icon2) {
		SDL_SetWindowIcon(video_.getWindow(), icon2);
	}
#endif

	return true;
}

#define BASENAME_DATA		"data.bin"

bool base_instance::load_data_bin()
{
	VALIDATE(!game_config::app.empty(), _("Must set game_config::app"));
	VALIDATE(!game_config::app_channel.empty(), _("Must set game_config::app_channel"));
	VALIDATE(core_cfg_.empty(), null_str);

	cache_.clear_defines();

	loadscreen::global_loadscreen_manager loadscreen_manager(video_);
	cursor::setter cur(cursor::WAIT);

	try {		
		wml_config_from_file(game_config::path + "/xwml/" + BASENAME_DATA, app_cfg_);
	
		// [terrain_type]
		const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
		BOOST_FOREACH (const config &t, terrains) {
			tmap::terrain_types.add_child("terrain_type", t);
		}
		app_cfg_.clear_children("terrain_type");

		// animation
		anim2::fill_anims(app_cfg_.child("units"));

		app_load_data_bin();

		// save this to core_cfg_
		core_cfg_ = app_cfg_;
		paths_manager_.set_paths(core_cfg_);

	} catch (game::error& e) {
		// ERR_CONFIG << "Error loading game configuration files\n";
		gui2::show_error_message(_("Error loading game configuration files: '") +
			e.message + _("' (The game will now exit)"));
		return false;
	}
	return true;
}

void base_instance::reload_data_bin(const config& data_cfg)
{
	// place execute to main thread.
	sdl_thread_.Invoke<void>(RTC_FROM_HERE, rtc::Bind(&base_instance::set_data_bin_cfg, this, data_cfg));
}

void base_instance::set_data_bin_cfg(const config& cfg)
{
	app_cfg_ = cfg;
	// [terrain_type]
	const config::const_child_itors& terrains = app_cfg_.child_range("terrain_type");
	BOOST_FOREACH (const config &t, terrains) {
		tmap::terrain_types.add_child("terrain_type", t);
	}
	app_cfg_.clear_children("terrain_type");

	// save this to core_cfg_
	core_cfg_ = app_cfg_;

	// conitnue to [rose_config].
	const config& core_cfg = instance->core_cfg();
	const config& game_cfg = core_cfg.child("rose_config");
	game_config::load_config(game_cfg? &game_cfg : NULL);
}

bool base_instance::change_language()
{
	std::vector<std::string> items;

	const std::vector<language_def>& languages = get_languages();
	const language_def& current_language = get_language();
	
	int initial_sel = 0;
	BOOST_FOREACH (const language_def& lang, languages) {
		items.push_back(lang.language);
		if (lang == current_language) {
			initial_sel = items.size() - 1;
		}
	}

	int cursel = nposm;
	if (game_config::svga) {
		gui2::tcombo_box dlg(items, initial_sel, _("Language"), true);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK || !dlg.dirty()) {
			return false;
		}

		cursel = dlg.cursel();

	} else {
		gui2::tcombo_box2 dlg(_("Language"), items, initial_sel);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK || !dlg.dirty()) {
			return false;
		}

		cursel = dlg.cursel();
	}

	::set_language(languages[cursel]);
	preferences::set_language(languages[cursel].localename);

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::wesnoth_version.str();
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());
	return true;
/*
	gui2::tlanguage_selection dlg;
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) return false;

	std::string wm_title_string = game_config::get_app_msgstr(null_str) + " - " + game_config::wesnoth_version.str();
	SDL_SetWindowTitle(video_.getWindow(), wm_title_string.c_str());	
	return true;
*/
}

int base_instance::show_preferences_dialog()
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	return gui2::twindow::OK;
#else
	std::vector<std::string> items;

	std::vector<int> values;
	int fullwindowed = preferences::fullscreen()? preferences::MAKE_WINDOWED: preferences::MAKE_FULLSCREEN;
	std::string str = preferences::fullscreen()? _("Exit fullscreen") : _("Enter fullscreen");
	items.push_back(str);
	values.push_back(fullwindowed);
	if (!preferences::fullscreen()) {
		items.push_back(_("Change Resolution"));
		values.push_back(preferences::CHANGE_RESOLUTION);
	}
	items.push_back(_("Close"));
	values.push_back(gui2::twindow::OK);

	gui2::tcombo_box dlg(items, nposm);
	dlg.show();
	if (dlg.cursel() == nposm) {
		return gui2::twindow::OK;
	}

	return values[dlg.cursel()];
#endif
}

void base_instance::clear_textures()
{
	gui2::clear_textures();
	image::flush_cache(true);
}

void base_instance::pre_create_renderer()
{
	if (rtc_client_) {
		rtc_client_->pre_create_renderer();
	}
	app_pre_create_renderer();
}

void base_instance::post_create_renderer()
{
	if (rtc_client_) {
		rtc_client_->post_create_renderer();
	}
	app_post_create_renderer();
}

void base_instance::fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg)
{
	if (area) {
		anims_.insert(std::make_pair(at, new animation(cfg)));
	} else {
		if (tpl) {
			utype_anim_tpls_.insert(std::make_pair(id, cfg));
		} else {
			anims_.insert(std::make_pair(at, new animation(cfg)));
		}
	}
}

void base_instance::clear_anims()
{
	utype_anim_tpls_.clear();

	for (std::map<int, animation*>::const_iterator it = anims_.begin(); it != anims_.end(); ++ it) {
		animation* anim = it->second;
		delete anim;
	}
	anims_.clear();
}

const animation* base_instance::anim(int at) const
{
	std::map<int, animation*>::const_iterator i = anims_.find(at);
	if (i != anims_.end()) {
		return i->second;
	}
	return NULL;
}

void base_instance::prefix_create(const std::string& app, const std::string& channel, bool landscape)
{
	std::stringstream err;
	// if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) < 0) {
	if (SDL_Init(SDL_INIT_TIMER) < 0) {
		err << "Couldn't initialize SDL: " << SDL_GetError();
		throw twml_exception(err.str());
	}

	srand((unsigned int)time(NULL));
	game_config::init(app, channel, landscape);
}

void base_instance::initialize()
{
	// In order to reduce 'black screen' time, so as soon as possible to call init_video.
	bool res = init_video();
	if (!res) {
		throw twml_exception("could not initialize display");
	}

	// if fail, use throw twml_exception.
	init_locale();

	lobby = create_lobby();
	if (!lobby) {
		throw twml_exception("could not create lobby");
	}

	// do initialize fonts before reading the game config, to have game
	// config error messages displayed. fonts will be re-initialized later
	// when the language is read from the game config.
	res = font::load_font_config();
	if (!res) {
		throw twml_exception("could not initialize fonts");
	}

	res = init_language();
	if (!res) {
		throw twml_exception("could not initialize the language");
	}

	cursor_manager_ = new cursor::manager;
	cursor::set(cursor::WAIT);

	// in order to 
	loadscreen_manager_ = new loadscreen::global_loadscreen_manager(video_);
	loadscreen::start_stage("titlescreen");

	res = gui2::init();
	if (!res) {
		throw twml_exception("could not initialize gui2-subsystem");
	}
	gui2_event_manager_ = new gui2::event::tmanager;

	// load core config require some time, so execute after gui2::init.
	res = load_data_bin();
	if (!res) {
		throw twml_exception("could not initialize game config");
	}

	app_load_pb();

	std::pair<std::string, std::string> desire_theme = utils::split_app_prefix_id(preferences::theme());
	theme_switch_to(desire_theme.first, desire_theme.second);
}

void base_instance::close()
{
	// in reverse order compaire to initialize 
	if (gui2_event_manager_) {
		delete gui2_event_manager_;
		gui2_event_manager_ = NULL;
	}

	if (loadscreen_manager_) {
		delete loadscreen_manager_;
		loadscreen_manager_ = NULL;
	}

	if (cursor_manager_) {
		delete cursor_manager_;
		cursor_manager_ = NULL;
	}

	if (lobby) {
		delete lobby;
		lobby = NULL;
	}
}

void base_instance::theme_switch_to(const std::string& app, const std::string& id)
{
	if (!theme::current_id.empty() && theme::current_id == utils::generate_app_prefix_id(app, id)) {
		return;
	}

	const config* default_theme_cfg = nullptr;
	const config* theme_cfg = nullptr;
	BOOST_FOREACH(const config& cfg, core_cfg_.child_range("theme")) {
		if (cfg["app"].str() == app && cfg["id"].str() == id) {
			theme_cfg = &cfg;
			break;
		} else if (cfg["app"].str() == "rose" && cfg["id"].str() == "default") {
			default_theme_cfg = &cfg;
		}
	}
	VALIDATE(theme_cfg != nullptr || default_theme_cfg != nullptr, null_str);
	theme::switch_to(theme_cfg? *theme_cfg: *default_theme_cfg);

	preferences::set_theme(theme::current_id);
}

tlobby* base_instance::create_lobby()
{
	return new tlobby(new tlobby::tchat_sock(), new tlobby::thttp_sock(), new tlobby::ttransit_sock());
}

void base_instance::pump()
{
	sdl_thread_.pump();
	lobby->pump();
}