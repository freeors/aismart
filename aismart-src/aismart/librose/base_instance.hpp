/* $Id: editor_display.hpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#ifndef LIBROSE_BASE_INSTANCE_HPP_INCLUDED
#define LIBROSE_BASE_INSTANCE_HPP_INCLUDED

#include "rose_config.hpp"
#include "font.hpp"
#include "area_anim.hpp"
#include "hero.hpp"
#include "preferences.hpp"
#include "sound.hpp"
#include "filesystem.hpp"
#include "serialization/preprocessor.hpp"
#include "config_cache.hpp"
#include "cursor.hpp"
#include "loadscreen.hpp"
#include "lobby.hpp"

// protobuf
#include <google/protobuf/message.h>

// webrtc
#include <rtc_base/thread.h>
#include <rtc_base/physicalsocketserver.h>

// chromium
#include <base/test/scoped_task_environment.h>

#define INVALID_UINT32_ID		0

class animation;
class trtc_client;

namespace rtc {
// SDLThread. Automatically pumps wakeup and IO messages.

class SDLThread : public Thread
{
public:
	SDLThread(PhysicalSocketServer* ss)
		: Thread(ss)
	{
	}
	virtual ~SDLThread()
	{
		// Stop();
	}
	void pump()
	{
		Message msg;
		size_t max_msgs = std::max<size_t>(1, size());
		// require detect io signal, so io_process of Get use true.
		for (; max_msgs > 0 && Get(&msg, 0, true); --max_msgs) {
			Dispatch(&msg);
		}
	}
};

}

class base_instance
{
public:
	static void prefix_create(const std::string& _app, const std::string& channel, bool landscape);

	base_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv, int sample_rate = 44100, size_t sound_buffer_size = 4096);
	virtual ~base_instance();

	void initialize();
	virtual void close();
	virtual tlobby* create_lobby();

	loadscreen::global_loadscreen_manager& loadscreen_manager() const { return *loadscreen_manager_; }

	bool init_language();
	bool init_video();
	bool load_data_bin();

	virtual void app_load_pb() {}
	virtual std::pair<std::string, ::google::protobuf::Message*> app_pblite_from_type(int type) { return std::make_pair(null_str, nullptr); }

	/** Gets the underlying screen object. */
	CVideo& video() { return video_; }

	const config& app_cfg() const { return app_cfg_; }
	const config& core_cfg() const { return core_cfg_; }

	// it is called by studio, other app don't use it.
	void reload_data_bin(const config& data_cfg);
	void set_data_bin_cfg(const config& cfg);

	bool is_loading() { return false; }

	bool change_language();
	virtual int show_preferences_dialog();

	virtual void regenerate_heros(hero_map& heros, bool allow_empty);
	hero_map& heros() { return heros_; }

	virtual void app_fill_anim_tags(std::map<const std::string, int>& tags) {};
	virtual void fill_anim(int at, const std::string& id, bool area, bool tpl, const config& cfg);

	void pre_create_renderer();
	void post_create_renderer();
	void clear_textures();

	void clear_anims();
	const std::multimap<std::string, const config>& utype_anim_tpls() const { return utype_anim_tpls_; }
	const animation* anim(int at) const;

	void init_locale();
	void handle_app_event(Uint32 type);
	void handle_window_event(Uint32 type);

	bool foreground() const { return foreground_; }
	bool terminating() const { return terminating_; }
	bool minimized() const { return minimized_; }

	void theme_switch_to(const std::string& app, const std::string& id);

	uint32_t get_callback_id() const;
	uint32_t background_connect(const boost::function<bool (uint32_t ticks, bool screen_on)>& callback);
	void handle_background(bool screen_on);

	void set_rtc_client(trtc_client* chat) { rtc_client_ = chat; }
	trtc_client* rtc_client() const { return rtc_client_; }

	rtc::SDLThread& sdl_thread() { return sdl_thread_; }
	void pump();

protected:
	virtual void app_load_settings_config(const config& cfg) {}

private:
	virtual void app_init_locale(const std::string& intl_dir) {}
	virtual void app_load_data_bin() {}

	virtual void app_terminating() {}
	virtual void app_willenterbackground() {}
	virtual void app_didenterbackground() {}
	virtual void app_willenterforeground() {}
	virtual void app_didenterforeground() {}
	virtual void app_lowmemory() {}

	virtual void app_pre_create_renderer() {}
	virtual void app_post_create_renderer() {}

	void background_disconnect(const uint32_t id);
	void background_disconnect_th(const uint32_t id);

protected:
	surface icon_;
	CVideo video_;
	hero_map heros_;

	base::test::ScopedTaskEnvironment chromium_env_;
	const font::manager font_manager_;
	const preferences::base_manager prefs_manager_;
	const image::manager image_manager_;
	sound::music_thinker music_thinker_;
	binary_paths_manager paths_manager_;
	anim2::manager anim2_manager_;
	const cursor::manager* cursor_manager_; // it should create after video-subsystem.
	loadscreen::global_loadscreen_manager* loadscreen_manager_; // it should create after video-subsystem.
	const gui2::event::tmanager* gui2_event_manager_; // it should create after gui2-subsystem.

	config app_cfg_;
	config core_cfg_; // remove [terrain_type] children's data.bin.

	preproc_map old_defines_map_;
	config_cache& cache_;

	bool foreground_;
	bool terminating_;
	bool minimized_;
	std::map<int, animation*> anims_;
	std::multimap<std::string, const config> utype_anim_tpls_;

	boost::scoped_ptr<sound::tpersist_xmit_audio_lock> background_persist_xmit_audio_;
	int background_callback_id_;
	std::map<uint32_t, boost::function<bool (uint32_t ticks, bool screen_on)> > background_callbacks_;

	trtc_client* rtc_client_;
	rtc::SDLThread sdl_thread_;
};

extern base_instance* instance;

// C++ Don't call virtual function during class construct function.
template<class T>
class instance_manager
{
public:
	instance_manager(rtc::PhysicalSocketServer& ss, int argc, char** argv, const std::string& app, const std::string& channel, bool landscape)
	{
		// if exception generated when construct, system don't call destructor.
		try {
			base_instance::prefix_create(app, channel, landscape);
			instance = new T(ss, argc, argv);
			instance->initialize();

		} catch(...) {
			if (instance) {
				delete instance;
				instance = NULL;
			}
			throw;
		}
	}
	
	~instance_manager()
	{
		if (instance) {
			delete instance;
			instance = NULL;
		}
	}
	T& get() { return *(dynamic_cast<T*>(instance)); }
};

#endif
