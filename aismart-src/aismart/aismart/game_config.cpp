/* $Id: game_config.cpp 46969 2010-10-08 19:45:32Z mordante $ */
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

#include "global.hpp"
#include "game_config.hpp"
#include "config_cache.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "filesystem.hpp"
#include "tflite.hpp"
#include "serialization/parser.hpp"

#include <sstream>

int script_parse_source(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.source must not empty.");
		return result;
	}
	if (value == "camera") {
		result = source_camera;

	} else {
		size_t pos = value.rfind(".png");
		if (pos != std::string::npos && pos + 4 == value.size()) {
			if (value.find('\\') == std::string::npos) {
				if (value[0] != '/') {
					result = source_img;

				} else {
					err_report = _("if tflite.source is image, it's first must not be '/'.");
					return result;
				}
			} else {
				err_report = _("if tflite.source is image, must not contain '\\'.");
				return result;
			}
		}
	}

	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.source: ") << value;
		err_report = err.str();
	}
	return result;
}

int script_parse_scenario(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.scenario must not empty.");
		return result;
	}
	if (value == "classifier") {
		result = scenario_classifier;

	} else if (value == "detect") {
		err_report = _("tflite.scenario doesn't support object detect.");
		return result;
		result = scenario_detect;

	} else if (value == "ocr") {
		result = scenario_ocr;

	} else if (value == "pr") {
		result = scenario_pr;
	}
	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.scenario: ") << value;
		err_report = err.str();
	}
	return result;
}

int script_parse_color(const std::string& value, std::string& err_report)
{
	int result = nposm;
	if (value.empty()) {
		err_report = _("tflite.color must not empty.");
		return result;
	}
	if (value == "gray") {
		result = tflite::color_gray;

	} else if (value == "rgb") {
		result = tflite::color_rgb;

	} else if (value == "bgr") {
		result = tflite::color_bgr;

	}
	if (result == nposm) {
		VALIDATE(err_report.empty(), null_str);
		std::stringstream err;
		err << _("Unknown tflite.color: ") << value;
		err_report = err.str();
	}
	return result;
}

bool tscript::from(const config& cfg, std::string& err_report)
{
	err_report.clear();

	tflite = cfg["tflite"].str();
	if (tflite.empty()) {
		err_report = _("tflite.tflite must be set.");
	}

	if (err_report.empty()) {
		source = script_parse_source(cfg["source"].str(), err_report);
		if (source == source_img) {
			img = cfg["source"].str();
		}
	}
	if (err_report.empty()) {
		scenario = script_parse_scenario(cfg["scenario"].str(), err_report);
	}
	if (err_report.empty()) {
		width = cfg["width"].to_int();
		if (width <= 0) {
			err_report = _("tflite.width must be > 0.");
		}
	}
	if (err_report.empty()) {
		height = cfg["height"].to_int();
		if (height <= 0) {
			err_report = _("tflite.height must be > 0.");
		}
	}
	if (err_report.empty()) {
		color = script_parse_color(cfg["color"].str(), err_report);
	}

	if (err_report.empty()) {
		kthreshold = cfg["kthreshold"].to_double(0.1);
		if (kthreshold < 0 || kthreshold > 1) {
			err_report = _("tflite.kthreshold must be (0, 1).");
		}
	}

	if (!err_report.empty()) {
		*this = null_script;
	}
	
	warn = cfg["warn"].str();

	return err_report.empty();
}

tscript::tscript(const std::string& file, bool res, std::string& err_report)
	: test(false)
	, res(res)
	, id()
	, source(nposm)
	, scenario(nposm)
	, width(0)
	, height(0)
	, color(nposm)
	, kthreshold(0)
{
	if (file.empty()) {
		return;
	}

	const size_t pos = file.rfind(".cfg");
	if (pos == std::string::npos || pos + 4 != file.size()) {
		return;
	}

	config script_cfg;
	{
		config_cache& cache = config_cache::instance();
		cache.get_config(file, script_cfg);
	}
	if (script_cfg.empty()) {
		return;
	}
	const config& tflite_cfg = script_cfg.child("script", 0);
	if (!tflite_cfg) {
		return;
	}

	id = extract_file(file);
	from(tflite_cfg, err_report);
}

const tscript null_script;

bool ttflite::aes_encrypt(const std::string& key) const
{
/*
	const int key_bytes = 32;
	VALIDATE(valid() && (int)key.size() == key_bytes, null_str);

	tfile src(tflits_path(), GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = src.read_2_data(sizeof(ttflits_header));

	const int encrypt_bytes = 512;
	std::unique_ptr<uint8_t> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	VALIDATE((encrypt_bytes % key_bytes == 0) && fsize >= encrypt_bytes, null_str);
	utils::aes_256_encrypt((const uint8_t*)key.c_str(), (uint8_t*)src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	SDL_memcpy(src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	ttflits_header header;
	SDL_memset(&header, 0, sizeof(ttflits_header));
	header.timestamp = time(nullptr);
	header.fourcc = mmioFOURCC('T', 'F', 'S', '1');
	SDL_memcpy(src.data, &header, sizeof(ttflits_header));

	tfile dst(tflits_path(), GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(dst.fp, src.data, sizeof(ttflits_header) + fsize);
*/
	return true;
}

bool ttflite::aes_decrypt(const std::string& key) const
{
/*
	const int key_bytes = 32;
	VALIDATE(valid() && (int)key.size() == key_bytes, null_str);

	tfile src(tflits_path(), GENERIC_READ, OPEN_EXISTING);
	int64_t fsize = src.read_2_data();

	const int encrypt_bytes = 512;
	VALIDATE(encrypt_bytes % key_bytes == 0, null_str);
	if (fsize < sizeof(ttflits_header) + encrypt_bytes) {
		return false;
	}
	ttflits_header header;
	SDL_memcpy(&header, src.data, sizeof(ttflits_header));
	if (header.fourcc != mmioFOURCC('T', 'F', 'S', '1')) {
		return false;
	}


	std::unique_ptr<uint8_t> out(new uint8_t[encrypt_bytes]);
	uint8_t* out_ptr = out.get();
	VALIDATE((encrypt_bytes % key_bytes == 0) && fsize >= sizeof(ttflits_header) + encrypt_bytes, null_str);
	utils::aes_256_decrypt((uint8_t*)key.c_str(), (uint8_t*)src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);
	SDL_memcpy(src.data + sizeof(ttflits_header), out_ptr, encrypt_bytes);

	std::string path2 = path() + "/" + id + "_dec.tflite";
	tfile dst(path2, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(dst.fp, src.data + sizeof(ttflits_header), fsize - sizeof(ttflits_header));
*/
	return true;
}

std::map<std::string, std::string> treadme::support_keywords;
std::map<std::string, std::string> treadme::support_states;

void treadme::prepare_stats_keywords()
{
	if (support_keywords.empty()) {
		support_keywords.insert(std::make_pair("cnn", _("CNN")));
		support_keywords.insert(std::make_pair("rnn", _("RNN")));
		support_keywords.insert(std::make_pair("mobilenets", _("MobileNets")));
		support_keywords.insert(std::make_pair("inception_v3", _("Inception V3")));
		support_keywords.insert(std::make_pair("inception_v5", _("Inception V5")));
		support_keywords.insert(std::make_pair("vgg", _("VGG")));
		support_keywords.insert(std::make_pair("lstm", _("LSTM")));
		support_keywords.insert(std::make_pair("smart_reply", _("Smart Replay")));
	}

	if (support_states.empty()) {
		support_states.insert(std::make_pair("debugging", _("Debugging")));
		support_states.insert(std::make_pair("training", _("Training")));
		support_states.insert(std::make_pair("commercially", _("Commercially")));
	}
}

const std::string& treadme::keyword_name(const std::string& id)
{
	std::map<std::string, std::string>::const_iterator it = support_keywords.find(id);
	if (it != support_keywords.end()) {
		return it->second;
	}
	return null_str;
}

std::string treadme::keywords_name(const std::vector<std::string>& ids)
{
	std::stringstream ret;
	for (std::vector<std::string>::const_iterator it = ids.begin(); it != ids.end(); ++ it) {
		if (it != ids.begin()) {
			ret << ", ";
		}
		ret << keyword_name(*it);
	}
	return ret.str();
}

const std::string& treadme::state_name(const std::string& id)
{
	std::map<std::string, std::string>::const_iterator it = support_states.find(id);
	if (it != support_states.end()) {
		return it->second;
	}
	return null_str;
}

void tnetwork_item::set_model_special(const std::string& _token, const int64_t _timestamp, const std::string& state_id, const std::string& keywords_id, const std::string& _desc, const bool _showcase, const int _follows, const int _comments)
{
	token = _token;
	timestamp = _timestamp;

	keywords.clear();
	std::vector<std::string> vstr = utils::split(keywords_id);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		if (treadme::support_keywords.count(id)) {
			keywords.push_back(id);
		}
	}

	state.empty();
	if (treadme::support_states.count(state_id)) {
		state = state_id;
	}

	description = _desc;
	showcase = _showcase;
	follows = _follows;
	comments = _comments;
}

ttoken_tflite::ttoken_tflite(const std::string& token, const int timestamp, const std::string& name, const bool followed, const std::string& nickname, const std::string& keywords_id, const std::string& state_id, const std::string& desc, int follows, int comments, int size)
	: token(token)
	, timestamp(timestamp)
	, name(name)
	, followed(followed)
	, nickname(nickname)
	, description(desc)
	, follows(follows)
	, comments(comments)
	, size(size)
{
	keywords.clear();
	std::vector<std::string> vstr = utils::split(keywords_id);
	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		if (treadme::support_keywords.count(id)) {
			keywords.push_back(id);
		}
	}

	state.empty();
	if (treadme::support_states.count(state_id)) {
		state = state_id;
	}
}

namespace game_config {

const std::string aismart_key = "abcdefghijklmnop";
const std::string showcase_app_secret = "01234567890123456789012345678901";
std::string showcase_sha256;

}


namespace preferences {

std::string script()
{
	return preferences::get("script");
}

void set_script(const std::string& value)
{
	if (script() != value) {
		preferences::set("script", value);
		preferences::write_preferences();
	}
}

std::string bbs_server_furl()
{
	return preferences::get("bbs_server_furl");
}

void set_bbs_server_furl(const std::string& furl)
{
	VALIDATE(!furl.empty(), null_str);
	if (bbs_server_furl() != furl) {
		preferences::set("bbs_server_furl", furl);
		preferences::write_preferences();
	}
}

}