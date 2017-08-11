/* Require Rose v1.0.10 or above. $ */

#define GETTEXT_DOMAIN "aismart-lib"

#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/dialogs/login.hpp"
#include "gui/dialogs/register.hpp"
#include "gui/dialogs/resetpwd.hpp"
#include "gui/dialogs/user_manager.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"
#include "version.hpp"
#include "net.hpp"

#include <boost/bind.hpp>

namespace easypr {
std::string kDefaultSvmPath;
std::string kLBPSvmPath;
std::string kHistSvmPath;

std::string kDefaultAnnPath;
std::string kChineseAnnPath;
std::string kGrayAnnPath;

//This is important to for key transform to chinese
std::string kChineseMappingPath;
}

class game_instance: public base_instance
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

	void handle_login();
	void handle_ocr(const tscript& script);

	tscript& script() { return script_; }
	std::map<std::string, std::string>& private_keys() { return private_keys_; }

private:
	void app_load_settings_config(const config& cfg) override;
	void app_load_pb() override;

private:
	tscript script_;
	std::map<std::string, std::string> private_keys_;
};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
{
}

void game_instance::app_load_settings_config(const config& cfg)
{
	game_config::version = cfg["version"].str();
	game_config::wesnoth_version = version_info(game_config::version);

	const config& bbs_server_cfg = cfg.child("bbs_server");
	game_config::bbs_server.from_cfg(bbs_server_cfg? bbs_server_cfg: null_cfg);

	game_config::showcase_sha256 = tflite::sha256(game_config::aismart_key, game_config::showcase_app_secret);
}

void game_instance::handle_login()
{
	gui2::tlogin::tresult res;
	while (true) {
		{
			gui2::tlogin dlg;
			dlg.show();
			res = static_cast<gui2::tlogin::tresult>(dlg.get_retval());
		}
		if (res == gui2::tlogin::REGISTER) {
			gui2::tregister dlg;
			dlg.show();
			if (dlg.get_retval() == gui2::twindow::OK) {
				break;
			}


		} else if (res == gui2::tlogin::RESETPWD) {
			gui2::tresetpwd dlg(false);
			dlg.show();
			if (dlg.get_retval() == gui2::twindow::OK) {
				break;
			}


		} else {
			break;
		}
	}
}

void game_instance::handle_ocr(const tscript& script)
{
	const std::string pb_short_path = "combined_model.pb";

	std::vector<std::string> fields;
	for (int n = 0; n < 5; n ++) {
		fields.push_back(std::string(_("Field")) + "#" + str_cast(n));
	}
	surface surf;
	if (script.source == source_img) {
		surf = image::get_image(script.img);
	}
	tflite::tparams params(script.width, script.height, script.color);
	std::map<std::string, tocr_result> ret = tflite::ocr(app_cfg(), video_, surf, fields, script.tflite, game_config::showcase_sha256, params, false);
	if (ret.empty()) {
		return;
	}

	std::stringstream msg_ss;
	for (std::map<std::string, tocr_result>::const_iterator it = ret.begin(); it != ret.end(); ++ it) {
		if (!msg_ss.str().empty()) {
			msg_ss << "\n";
		}
		msg_ss << it->first << ": " << it->second.chars << ", used ms:" << it->second.used_ms;
	}
	gui2::show_message(null_str, msg_ss.str());
}

void copy_model_directory(const std::string& src_dir)
{
	Uint32 start = SDL_GetTicks();

	// copy font directory from res/model to user_data_dir/model
	std::vector<std::string> v;
	v.push_back("svm_hist.xml");
	v.push_back("svm_lbp.xml");
	v.push_back("svm_hist.xml");
	v.push_back("ann.xml");
	v.push_back("ann_chinese.xml");
	v.push_back("annCh.xml");
	v.push_back("province_mapping");
	create_directory_if_missing(game_config::preferences_dir + "/model");
	for (std::vector<std::string>::const_iterator it = v.begin(); it != v.end(); ++ it) {
		const std::string src = src_dir + "/" + *it;
		const std::string dst = game_config::preferences_dir + "/model/" + *it;
		if (!file_exists(dst)) {
			tfile src_file(src, GENERIC_READ, OPEN_EXISTING);
			tfile dst_file(dst, GENERIC_WRITE, CREATE_ALWAYS);
			if (src_file.valid() && dst_file.valid()) {
				int32_t fsize = src_file.read_2_data();
				posix_fwrite(dst_file.fp, src_file.data, fsize);
			}
		}
	}
}

void game_instance::app_load_pb()
{
	// copy models from res to preferences
	const std::string src = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/model";
	const std::string dst = game_config::preferences_dir;
	copy_model_directory(src);

	easypr::kDefaultSvmPath = game_config::preferences_dir + "/model/svm_hist.xml";
	easypr::kLBPSvmPath = game_config::preferences_dir + "/model/svm_lbp.xml";
	easypr::kHistSvmPath = game_config::preferences_dir + "/model/svm_hist.xml";

	easypr::kDefaultAnnPath = game_config::preferences_dir + "/model/ann.xml";
	easypr::kChineseAnnPath = game_config::preferences_dir + "/model/ann_chinese.xml";
	easypr::kGrayAnnPath = game_config::preferences_dir + "/model/annCh.xml";

	//This is important to for key transform to chinese
	easypr::kChineseMappingPath = game_config::preferences_dir + "/model/province_mapping";

#ifdef _WIN32
	conv_ansi_utf8(easypr::kDefaultSvmPath, false);
	conv_ansi_utf8(easypr::kLBPSvmPath, false);
	conv_ansi_utf8(easypr::kHistSvmPath, false);

	conv_ansi_utf8(easypr::kDefaultAnnPath, false);
	conv_ansi_utf8(easypr::kChineseAnnPath, false);
	conv_ansi_utf8(easypr::kGrayAnnPath, false);

	conv_ansi_utf8(easypr::kChineseMappingPath, false);
#endif

	const std::string furl = preferences::bbs_server_furl();
	if (!furl.empty()) {
		game_config::bbs_server.split_furl(furl, nullptr);
	}
}

/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "aismart", "#rose", false);
	game_instance& game = manager.get();

	try {
		lobby->disable_chat(); // because irc nick protocol, disable chat.
		treadme::prepare_stats_keywords();

		std::map<std::string, std::string>& private_keys = game.private_keys();

		if (preferences::startup_login()) {
			// login
			std::string mobile = group.leader().name();
			std::string password = preferences::password();
			int type = preferences::openid_type();

			if (!mobile.empty()) {
				tuser user;
				gui2::run_with_progress(null_str, boost::bind(&net::do_login, mobile, password, true, boost::ref(user)), false, 3000, false);
				if (user.valid()) {
					current_user = user;
				}
			}
		}

		int start_layer = gui2::thome::TEST_LAYER;

		for (;;) {
			game.loadscreen_manager().reset();
			const font::floating_label_context label_manager;

			cursor::set(cursor::NORMAL);

			gui2::thome::tresult res;
			{
				gui2::thome dlg(game.app_cfg(), game.script(), start_layer);
				dlg.show();
				res = static_cast<gui2::thome::tresult>(dlg.get_retval());
			}

			if (res == gui2::thome::USER_MANAGER) {
				gui2::tuser_manager dlg(private_keys);
				dlg.show();
				start_layer = gui2::thome::DEVICE_LAYER;

			} else if (res == gui2::thome::LOGIN) {
				game.handle_login();
				start_layer = gui2::thome::MORE_LAYER;

			} else if (res == gui2::thome::OCR) {
				game.handle_ocr(game.script());
				start_layer = gui2::thome::TEST_LAYER;

			} else if (res == gui2::thome::VIDEO_RESOLUTION) {
				start_layer = gui2::thome::MORE_LAYER;

			} else if (res == gui2::thome::VIP) {
				start_layer = gui2::thome::MORE_LAYER;

			} else if (res == gui2::thome::CHAT) {
				gui2::tchat2 dlg("chat_module");
				dlg.show();
				start_layer = gui2::thome::MORE_LAYER;

			} else if (res == gui2::thome::LANGUAGE) {
				if (game.change_language()) {
					start_layer = gui2::thome::MORE_LAYER;
					t_string::reset_translations();
					image::flush_cache();
				}

			} 
		}

	} catch (twml_exception& e) {
		e.show();

	} catch (CVideo::quit&) {
		//just means the game should quit
		SDL_Log("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		SDL_SimplerMB("%s\n", e.user_message.c_str());
	}

	return 0;
}