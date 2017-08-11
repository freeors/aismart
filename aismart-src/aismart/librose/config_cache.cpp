/* $Id: config_cache.cpp 46186 2010-09-01 21:12:38Z silene $ */
/*
   Copyright (C) 2008 - 2010 by Pauli Nieminen <paniemin@cc.hut.fi>
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

#include "config_cache.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "serialization/parser.hpp"

#include "loadscreen.hpp"
#include "builder.hpp"
#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"

	config_cache& config_cache::instance()
	{
		static config_cache cache;
		return cache;
	}

	config_cache::config_cache() :
		defines_map_()
	{
		// To set-up initial defines map correctly
		clear_defines();
	}

	struct output {
		void operator()(const preproc_map::value_type& def)
		{
			// "key: " << def.first << " " << def.second;
		}
	};
	const preproc_map& config_cache::get_preproc_map() const
	{
		return defines_map_;
	}

	void config_cache::clear_defines()
	{
		defines_map_.clear();
		// set-up default defines map

#if defined(__APPLE__)
		defines_map_["APPLE"] = preproc_define();
#endif

	}

	void config_cache::get_config(const std::string& path, config& cfg)
	{
		// Make sure that we have fake transaction if no real one is going on
		fake_transaction fake;

		preproc_map copy_map(make_copy_map());
		read_configs(path, cfg, copy_map);
		add_defines_map_diff(copy_map);
	}

	preproc_map& config_cache::make_copy_map()
	{
		return config_cache_transaction::instance().get_active_map(defines_map_);
	}


	static bool compare_define(const preproc_map::value_type& a, const preproc_map::value_type& b)
	{
		if (a.first < b.first)
			return true;
		if (b.first < a.first)
			return false;
		if (a.second < b.second)
			return true;
		return false;
	}

	void config_cache::add_defines_map_diff(preproc_map& defines_map)
	{
		return config_cache_transaction::instance().add_defines_map_diff(defines_map);
	}


	void config_cache::read_configs(const std::string& path, config& cfg, preproc_map& defines_map)
	{
		//read the file and then write to the cache
		scoped_istream stream = preprocess_file(path, &defines_map);
		read(cfg, *stream);
	}

	void config_cache::recheck_filetree_checksum()
	{
		data_tree_checksum(true);
	}

	void config_cache::add_define(const std::string& define)
	{
		defines_map_[define] = preproc_define();
		if (config_cache_transaction::is_active())
		{
			// we have to add this to active map too
			config_cache_transaction::instance().get_active_map(defines_map_).insert(
					std::make_pair(define, preproc_define()));
		}

	}

	void config_cache::remove_define(const std::string& define)
	{
		// removing define: define;
		defines_map_.erase(define);
		if (config_cache_transaction::is_active())
		{
			// we have to remove this from active map too
			config_cache_transaction::instance().get_active_map(defines_map_).erase(define);
		}
	}

	config_cache_transaction::state config_cache_transaction::state_ = FREE;
	config_cache_transaction* config_cache_transaction::active_ = 0;

	config_cache_transaction::config_cache_transaction()
		: define_filenames_()
		, active_map_()
	{
		assert(state_ == FREE);
		state_ = NEW;
		active_ = this;
	}

	config_cache_transaction::~config_cache_transaction()
	{
		state_ = FREE;
		active_ = 0;
	}

	void config_cache_transaction::lock()
	{
		state_ = LOCKED;
	}

	const config_cache_transaction::filenames& config_cache_transaction::get_define_files() const
	{
		return define_filenames_;
	}

	void config_cache_transaction::add_define_file(const std::string& file)
	{
		define_filenames_.push_back(file);
	}

	preproc_map& config_cache_transaction::get_active_map(const preproc_map& defines_map)
	{
		if(active_map_.empty())
		{
			std::copy(defines_map.begin(),
					defines_map.end(),
					std::insert_iterator<preproc_map>(active_map_, active_map_.begin()));
			if ( get_state() == NEW)
				state_ = ACTIVE;
		 }

		return active_map_;
	}

	void config_cache_transaction::add_defines_map_diff(preproc_map& new_map)
	{

		if (get_state() == ACTIVE)
		{
			preproc_map temp;
			std::set_difference(new_map.begin(),
					new_map.end(),
					active_map_.begin(),
					active_map_.end(),
					std::insert_iterator<preproc_map>(temp,temp.begin()),
					&compare_define);

			BOOST_FOREACH (const preproc_map::value_type &def, temp) {
				insert_to_active(def);
			}

			temp.swap(new_map);
		} else if (get_state() == LOCKED) {
			new_map.clear();
		}
	}

	void config_cache_transaction::insert_to_active(const preproc_map::value_type& def)
	{
		active_map_[def.first] = def.second;
	}



teditor_::wml2bin_desc::wml2bin_desc()
	: bin_name()
	, wml_nfiles(0)
	, wml_sum_size(0)
	, wml_modified(0)
	, bin_nfiles(0)
	, bin_sum_size(0)
	, bin_modified(0)
	, require_build(false)
{}

void teditor_::wml2bin_desc::refresh_checksum(const std::string& working_dir)
{
	VALIDATE(valid(), null_str);
	std::string bin_file = working_dir + "/xwml/";
	if (!app.empty()) {
		bin_file += game_config::generate_app_dir(app) + "/";
	}
	bin_file += bin_name;
	wml_checksum_from_file(bin_file, &bin_nfiles, &bin_sum_size, (uint32_t*)&bin_modified);
}

#define BASENAME_DATA		"data.bin"
#define BASENAME_GUI		"gui.bin"
#define BASENAME_LANGUAGE	"language.bin"

// file processor function only support prefixed with game_config::path.
int teditor_::tres_path_lock::deep = 0;
teditor_::tres_path_lock::tres_path_lock(teditor_& o)
	: original_(game_config::path)
{
	VALIDATE(!deep, null_str);
	deep ++;
	game_config::path = o.working_dir_;
}

teditor_::tres_path_lock::~tres_path_lock()
{
	game_config::path = original_;
	deep --;
}

teditor_::teditor_(const std::string& working_dir) 
	: cache_(config_cache::instance())
	, working_dir_(working_dir)
	, wml2bin_descs_()
{
}

void teditor_::set_working_dir(const std::string& dir)
{
	if (working_dir_ == dir) {
		return;
	}
	working_dir_ = dir;
}

bool teditor_::make_system_bins_exist()
{
	std::string file;
	std::vector<BIN_TYPE> system_bins;
	for (BIN_TYPE type = BIN_MIN; type <= BIN_SYSTEM_MAX; type = (BIN_TYPE)(type + 1)) {
		if (type == MAIN_DATA) {
			file = working_dir_ + "/xwml/" + BASENAME_DATA;
		} else if (type == GUI) {
			file = working_dir_ + "/xwml/" + BASENAME_GUI;
		} else if (type == LANGUAGE) {
			file = working_dir_ + "/xwml/" + BASENAME_LANGUAGE;
		} else {
			VALIDATE(false, null_str);
		}
		if (!file_exists(file)) {
			system_bins.push_back(type);
		}
	}
	if (system_bins.empty()) {
		return true;
	}
	get_wml2bin_desc_from_wml(system_bins);
	const std::vector<std::pair<BIN_TYPE, wml2bin_desc> >& descs = wml2bin_descs();

	int count = (int)descs.size();
	for (int at = 0; at < count; at ++) {
		const std::pair<BIN_TYPE, wml2bin_desc>& desc = descs[at];

		bool ret = false;
		try {
			ret = cfgs_2_cfg(desc.first, desc.second.bin_name, desc.second.app, true, desc.second.wml_nfiles, desc.second.wml_sum_size, (uint32_t)desc.second.wml_modified);
		} catch (twml_exception& /*e*/) {
			return false;
		}
		if (!ret) {
			return false;
		}
	}

	return true;
}

// check location:
//   1. heros_army of artifcal
//   2. service_heros of artifcal
//   3. wander_heros of artifcal
//   4. heros_army of unit
std::string teditor_::check_scenario_cfg(const config& scenario_cfg)
{
	std::set<std::string> holded_str;
	std::set<int> holded_number;
	std::set<std::string> officialed_str;
	std::map<std::string, std::set<std::string> > officialed_map;
	std::map<std::string, std::string> mayor_map;
	int number;
	std::vector<std::string> str_vec;
	std::vector<std::string>::const_iterator tmp;
	std::stringstream str;

	BOOST_FOREACH (const config& side, scenario_cfg.child_range("side")) {
		const std::string leader = side["leader"];
		BOOST_FOREACH (const config& art, side.child_range("artifical")) {
			officialed_str.clear();
			const std::string cityno = art["cityno"].str();
			mayor_map[cityno] = art["mayor"].str();

			str_vec = utils::split(art["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["service_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
				officialed_str.insert(*tmp);
			}
			str_vec = utils::split(art["wander_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			officialed_map[cityno] = officialed_str;
		}
		BOOST_FOREACH (const config& u, side.child_range("unit")) {
			const std::string cityno = u["cityno"].str();
			std::map<std::string, std::set<std::string> >::iterator find_it = officialed_map.find(cityno);
			if (cityno != "0" && find_it == officialed_map.end()) {
				str << "." << scenario_cfg["id"].str() << ", heros_army=" << u["heros_army"].str() << " uses undefined cityno: " << cityno << "";
				return str.str();
			}
			str_vec = utils::split(u["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "." << scenario_cfg["id"].str() << ", hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
				if (find_it != officialed_map.end()) {
					find_it->second.insert(*tmp);
				}
			}
		}
		for (std::map<std::string, std::set<std::string> >::const_iterator it = officialed_map.begin(); it != officialed_map.end(); ++ it) {
			std::map<std::string, std::string>::const_iterator mayor_it = mayor_map.find(it->first);
			if (mayor_it->second.empty()) {
				continue;
			}
			if (mayor_it->second == leader) {
				str << "." << scenario_cfg["id"].str() << ", in cityno=" << it->first << " mayor(" << mayor_it->second << ") cannot be leader!";
				return str.str();
			}
			if (it->second.find(mayor_it->second) == it->second.end()) {
				str << "." << scenario_cfg["id"].str() << ", in ciytno=" << it->first << " mayor(" << mayor_it->second << ") must be in offical hero!";
				return str.str();
			}
		}
	}
	return "";
}

// check location:
//   1. heros_army of artifcal
//   2. service_heros of artifcal
//   3. wander_heros of artifcal
std::string teditor_::check_mplayer_bin(const config& mplayer_cfg)
{
	std::set<std::string> holded_str;
	std::set<int> holded_number;
	int number;
	std::vector<std::string> str_vec;
	std::vector<std::string>::const_iterator tmp;
	std::stringstream str;

	BOOST_FOREACH (const config& faction, mplayer_cfg.child_range("faction")) {
		BOOST_FOREACH (const config& art, faction.child_range("artifical")) {
			str_vec = utils::split(art["heros_army"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["service_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
			str_vec = utils::split(art["wander_heros"]);
			for (tmp = str_vec.begin(); tmp != str_vec.end(); ++ tmp) {
				if (holded_str.count(*tmp)) {
					str << "hero number: " << *tmp << " is conflicted!";
					return str.str();
				}
				number = lexical_cast_default<int>(*tmp);
				if (holded_number.count(number)) {
					str << "hero number: " << *tmp << " is invalid!";
					return str.str();
				}
				holded_str.insert(*tmp);
				holded_number.insert(number);
			}
		}
	}
	return "";
}

// check location:
std::string teditor_::check_data_bin(const config& data_cfg)
{
	std::stringstream str;

	BOOST_FOREACH (const config& campaign, data_cfg.child_range("campaign")) {
		if (!campaign.has_attribute("id")) {
			str << "Compaign hasn't id!";
			return str.str();
		}
	}
	return "";
}

#define BINKEY_ID_CHILD			"id_child"
#define BINKEY_SCENARIO_CHILD	"scenario_child"
#define BINKEY_PATH				"path"
#define BINKEY_MACROS			"macros"

void teditor_::generate_app_bin_config()
{
	config bin_cfg;
	bool ret = true;
	std::stringstream ss;
	SDL_DIR* dir = SDL_OpenDir((working_dir_ + "/data").c_str());
	if (!dir) {
		return;
	}
	SDL_dirent2* dirent;
	
	campaigns_config_.clear();
	cache_.clear_defines();
	while ((dirent = SDL_ReadDir(dir))) {
		if (SDL_DIRENT_DIR(dirent->mode)) {
			std::string app = game_config::extract_app_from_app_dir(dirent->name);
			if (app.empty()) {
				continue;
			}
			const std::string bin_file = std::string(dir->directory) + "/" + dirent->name + "/bin.cfg";
			if (!file_exists(bin_file)) {
				continue;
			}
			cache_.get_config(bin_file, bin_cfg);
			VALIDATE(!bin_cfg[BINKEY_ID_CHILD].str().empty(), bin_file + " hasn't no 'id_child' key!");
			VALIDATE(!bin_cfg[BINKEY_SCENARIO_CHILD].str().empty(), bin_file + " hasn't no 'scenario_child' key!");
			VALIDATE(!bin_cfg[BINKEY_PATH].str().empty(), bin_file + " hasn't no 'path' key!");
			std::string path = working_dir_ + "/data/" + bin_cfg[BINKEY_PATH];

			config& sub = campaigns_config_.add_child("bin");
			cache_.get_config(path, sub);
			sub["app"] = app;
			sub[BINKEY_ID_CHILD] = bin_cfg[BINKEY_ID_CHILD].str();
			sub[BINKEY_SCENARIO_CHILD] = bin_cfg[BINKEY_SCENARIO_CHILD].str();
			sub[BINKEY_PATH] = bin_cfg[BINKEY_PATH].str();
			sub[BINKEY_MACROS] = bin_cfg[BINKEY_MACROS].str();
			cache_.clear_defines();
		}
	}
	SDL_CloseDir(dir);
}

void teditor_::reload_data_bin(const config& data_cfg)
{
	instance->reload_data_bin(data_cfg);
/*
	// place execute to main thread.
	instance->set_data_bin_cfg(data_cfg);

	const config& core_cfg = instance->core_cfg();
	const config& game_cfg = core_cfg.child("rose_config");
	game_config::load_config(game_cfg? &game_cfg : NULL);
*/
}

bool teditor_::cfgs_2_cfg(const BIN_TYPE type, const std::string& name, const std::string& app, bool write_file, uint32_t nfiles, uint32_t sum_size, uint32_t modified, const std::map<std::string, std::string>& app_domains)
{
	config tmpcfg;

	tres_path_lock lock(*this);
	config_cache_transaction main_transaction;

	try {
		cache_.clear_defines();

		if (type == TB_DAT) {
			VALIDATE(write_file, "write_file must be true when generate TB_DAT!");

			std::string str = name;
			const size_t pos_ext = str.rfind(".");
			str = str.substr(0, pos_ext);
			str = str.substr(terrain_builder::tb_dat_prefix.size());

			const config& tb_cfg = tbs_config_.find_child("tb", "id", str);
			cache_.add_define(tb_cfg["define"].str());
			cache_.get_config(working_dir_ + "/data/tb.cfg", tmpcfg);

			if (write_file) {
				const config& tb_parsed_cfg = tmpcfg.find_child("tb", "id", str);
				binary_paths_manager paths_manager(tb_parsed_cfg);
				terrain_builder(tb_parsed_cfg, nfiles, sum_size, modified);
			}

		} else if (type == SCENARIO_DATA) {
			std::string name_str = name;
			const size_t pos_ext = name_str.rfind(".");
			name_str = name_str.substr(0, pos_ext);

			const config& app_cfg = campaigns_config_.find_child("bin", "app", app);
			const config& campaign_cfg = app_cfg.find_child(app_cfg[BINKEY_ID_CHILD], "id", name_str);

			if (!campaign_cfg["define"].empty()) {
				cache_.add_define(campaign_cfg["define"].str());
			}

			if (!app_cfg[BINKEY_MACROS].empty()) {
				cache_.get_config(working_dir_ + "/data/" + app_cfg[BINKEY_MACROS], tmpcfg);
			}
			cache_.get_config(working_dir_ + "/data/" + app_cfg[BINKEY_PATH] + "/" + name_str, tmpcfg);

			const config& refcfg = tmpcfg.child(app_cfg[BINKEY_SCENARIO_CHILD]);
			// check scenario config valid
			BOOST_FOREACH (const config& scenario, refcfg.child_range("scenario")) {
				std::string err_str = check_scenario_cfg(scenario);
				if (!err_str.empty()) {
					throw game::error(std::string("<") + name + std::string(">") + err_str);
				}
			}

			if (write_file) {
				const std::string xwml_app_path = working_dir_ + "/xwml/" + game_config::generate_app_dir(app);
				SDL_MakeDirectory(xwml_app_path.c_str());
				wml_config_to_file(xwml_app_path + "/" + name, refcfg, nfiles, sum_size, modified, app_domains);
			}

		} else if (type == GUI) {
			// no pre-defined
			VALIDATE(write_file, "write_file must be true when generate GUI!");

			cache_.get_config(working_dir_ + "/data/gui", tmpcfg);
			if (write_file) {
				wml_config_to_file(working_dir_ + "/xwml/" + BASENAME_GUI, tmpcfg, nfiles, sum_size, modified, app_domains);
			}

		} else if (type == LANGUAGE)  {
			// no pre-defined
			VALIDATE(write_file, "write_file must be true when generate LANGUAGE!");

			cache_.get_config(working_dir_ + "/data/languages", tmpcfg);
			if (write_file) {
				wml_config_to_file(working_dir_ + "/xwml/" + BASENAME_LANGUAGE, tmpcfg, nfiles, sum_size, modified, app_domains);
			}
		} else if (type == EXTENDABLE)  {
			// no pre-defined
			VALIDATE(!write_file, "write_file must be true when generate EXTENDABLE!");

			// app's bin
			generate_app_bin_config();

			// terrain builder rule
			const std::string tb_cfg = working_dir_ + "/data/tb.cfg";
			if (file_exists(tb_cfg)) {
				cache_.get_config(tb_cfg, tbs_config_);
			}
		} else {
			// type == MAIN_DATA
			cache_.add_define("CORE");
			cache_.get_config(working_dir_ + "/data", tmpcfg);

			// check scenario config valid
			std::string err_str = check_data_bin(tmpcfg);
			if (!err_str.empty()) {
				throw game::error(std::string("<") + BASENAME_DATA + std::string(">") + err_str);
			}

			if (write_file) {
				wml_config_to_file(working_dir_ + "/xwml/" + BASENAME_DATA, tmpcfg, nfiles, sum_size, modified, app_domains);
			}

			// in order to safe, require sync with main-thread in ther future.
			reload_data_bin(tmpcfg);
		} 
	}
	catch (game::error& e) {
		gui2::show_error_message(_("Error loading game configuration files: '") + e.message + _("' (The game will now exit)"));
		return false;
	}
	return true;
}

void teditor_::reload_extendable_cfg()
{
	cfgs_2_cfg(EXTENDABLE, null_str, null_str, false);
	// cfgs_2_cfg will translate relative msgid without load textdomain.
	// result of cfgs_2_cfg used to known what campaign, not detail information.
	// To detail information, need load textdomain, so call t_string::reset_translations(), 
	// let next translate correctly.
	t_string::reset_translations();
}

std::vector<std::string> generate_tb_short_paths(const std::string& id, const config& cfg)
{
	std::stringstream ss;
	std::vector<std::string> short_paths;

	ss.str("");
	ss << "data/core/terrain-graphics-" << id;
	short_paths.push_back(ss.str());

	binary_paths_manager paths_manager(cfg);
	const std::vector<std::string>& paths = paths_manager.paths();
	if (paths.empty()) {
		ss.str("");
		ss << "data/core/images/terrain-" << id;
		short_paths.push_back(ss.str());
	} else {
		for (std::vector<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
			ss.str("");
			ss << *it << "images/terrain-" << id;
			short_paths.push_back(ss.str());
		}
	}

	return short_paths;
}

// @path: c:\kingdom-res\data
void teditor_::get_wml2bin_desc_from_wml(const std::vector<BIN_TYPE>& system_bin_types)
{
	tres_path_lock lock(*this);

	wml2bin_desc desc;
	file_tree_checksum dir_checksum;
	std::vector<std::string> defines;
	std::vector<std::string> short_paths;
	std::string bin_to_path = working_dir_ + "/xwml";

	std::vector<BIN_TYPE> bin_types = system_bin_types;

	// tb-[tile].dat
	std::vector<config> tbs;
	size_t tb_index = 0;
	BOOST_FOREACH (const config& cfg, tbs_config_.child_range("tb")) {
		tbs.push_back(cfg);
		bin_types.push_back(TB_DAT);
	}

	// search <data>/campaigns, and form [campaign].bin
	std::vector<tapp_bin> app_bins;
	size_t app_bin_index = 0;

	BOOST_FOREACH (const config& bcfg, campaigns_config_.child_range("bin")) {
		const std::string& key = bcfg[BINKEY_ID_CHILD].str();
		BOOST_FOREACH (const config& cfg, bcfg.child_range(key)) {
			app_bins.push_back(tapp_bin(cfg["id"].str(), bcfg["app"].str(), std::string("data/") + bcfg[BINKEY_PATH].str(), std::string("data/") + bcfg[BINKEY_MACROS].str()));
			bin_types.push_back(SCENARIO_DATA);
		}
	}

	wml2bin_descs_.clear();

	for (std::vector<BIN_TYPE>::const_iterator itor = bin_types.begin(); itor != bin_types.end(); ++ itor) {
		BIN_TYPE type = *itor;

		defines.clear();
		short_paths.clear();
		bool calculated_wml_checksum = false;
		bool use_cfg_checksum = true;

		// of couse, code should use get_config to get checksum. but it will cose more cpu.
		// and home of roes is cpu-sensitive.
		const bool use_should_method = false;

		int filter = SKIP_MEDIA_DIR;
		if (type == TB_DAT) {
			const std::string& id = tbs[tb_index]["id"].str();
			short_paths = generate_tb_short_paths(id, tbs[tb_index]);
			filter = 0;

			data_tree_checksum(short_paths, dir_checksum, filter);
			desc.wml_nfiles = dir_checksum.nfiles;
			desc.wml_sum_size = dir_checksum.sum_size;
			desc.wml_modified = dir_checksum.modified;

			SDL_dirent st;
			const std::string terrain_graphics_cfg = working_dir_ + "/data/core/terrain-graphics-" + id + ".cfg";
			if (SDL_GetStat(terrain_graphics_cfg.c_str(), &st)) {
				if (st.mtime > desc.wml_modified) {
					desc.wml_modified = st.mtime;
				}
				desc.wml_sum_size += st.size;
				desc.wml_nfiles ++;
			}
			calculated_wml_checksum = true;

			desc.bin_name = terrain_builder::tb_dat_prefix + id + ".dat";
			tb_index ++;

		} else if (type == SCENARIO_DATA) {
			tapp_bin& bin = app_bins[app_bin_index ++];
			if (use_should_method) {
				const config& app_cfg = campaigns_config_.find_child("bin", "app", bin.app);
				const config& campaign_cfg = app_cfg.find_child(app_cfg[BINKEY_ID_CHILD], "id", bin.id);

				if (!campaign_cfg["define"].empty()) {
					defines.push_back(campaign_cfg["define"].str());
				}

				if (!app_cfg[BINKEY_MACROS].empty()) {
					short_paths.push_back(working_dir_ + "/data/" + app_cfg[BINKEY_MACROS]);
				}
				short_paths.push_back(working_dir_ + "/data/" + app_cfg[BINKEY_PATH] + "/" + bin.id);
			} else {
				if (!bin.macros.empty()) {
					short_paths.push_back(bin.macros);
				}
				short_paths.push_back(bin.path + "/" + bin.id);
				use_cfg_checksum = false;
			}

			desc.bin_name = bin.id + ".bin";
			desc.app = bin.app;

			bin_to_path = working_dir_ + "/xwml/" + game_config::generate_app_dir(bin.app);

		} else if (type == GUI) {
			if (use_should_method) {
				short_paths.push_back(working_dir_ + "/data/gui");
			} else {
				short_paths.push_back("data/gui");
				use_cfg_checksum = false;
			}

			desc.bin_name = BASENAME_GUI;

		} else if (type == LANGUAGE) {
			short_paths.push_back(working_dir_ + "/data/languages");

			desc.bin_name = BASENAME_LANGUAGE;

		} else {
			// (type == MAIN_DATA)
			short_paths.push_back(working_dir_ + "/data");

			desc.bin_name = BASENAME_DATA;
		}

		if (!calculated_wml_checksum) {
			if (use_cfg_checksum) {
				data_tree_checksum(defines, short_paths, dir_checksum);
			} else {
				data_tree_checksum(short_paths, dir_checksum, filter);
			}
			desc.wml_nfiles = dir_checksum.nfiles;
			desc.wml_sum_size = dir_checksum.sum_size;
			desc.wml_modified = dir_checksum.modified;
		}

		if (!wml_checksum_from_file(bin_to_path + "/" + desc.bin_name, &desc.bin_nfiles, &desc.bin_sum_size, (uint32_t*)&desc.bin_modified)) {
			desc.bin_nfiles = desc.bin_sum_size = desc.bin_modified = 0;
		}

		wml2bin_descs_.push_back(std::pair<BIN_TYPE, wml2bin_desc>(type, desc));
	}

	return;
}