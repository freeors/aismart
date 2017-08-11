#ifndef GUI_DIALOGS_HOME_HPP_INCLUDED
#define GUI_DIALOGS_HOME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "tflite.hpp"
#include "game_config.hpp"

#include <opencv2/core.hpp>


namespace gui2 {

class tstack;
class tlistbox;
class ttoggle_panel;
class ttoggle_button;
class tgrid;
class ttrack;

class thome: public tdialog
{
public:
	enum tresult {USER_MANAGER = 1, OCR, LOGIN, VIDEO_RESOLUTION, LANGUAGE, VIP, CHAT};
	enum {TEST_LAYER, DEVICE_LAYER, NETWORK_LAYER, SHOWCASE_LAYER, MORE_LAYER};
	enum {COOKIE_SCRIPT, COOKIE_TFLITE = 1000};

	explicit thome(const config& app_cfg, tscript& script, int start_layer);
	~thome();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show(twindow& window) {}

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_body_changed(treport& report, ttoggle_button& widget);

	const tscript* script_exist_same_path(const std::string& path) const;
	void set_test_script(const tscript& script);

	int split_cookie(const int cookie, int& xmit_type) const;
	int tflites_exist_same_id(const std::string& id, bool res) const;
	void click_notification(int layer);

	int token_in_netdisk_tflites(const std::string& token) const;
	int token_in_discover_tflites(const std::string& token) const;

	//
	// test
	//
	void pre_test();
	void click_start();
	void fill_description_list(tgrid& layer, const tscript& script);

	//
	// device
	//
	void pre_device();
	void click_user_manager();

	void fill_tflites_list(tlistbox& list);

	void collect_tflite(const std::string& path, std::set<tscript>& scripts, std::set<ttflite>& tflites) const;
	bool did_collect_tflite(const std::string& dir, const SDL_dirent2* dirent, std::set<tscript>& scripts, std::set<ttflite>& files) const;

	void collect_image(const std::string& path, std::set<std::string>& images) const;
	bool did_collect_image(const std::string& dir, const SDL_dirent2* dirent, std::set<std::string>& images) const;

	bool did_local_tflite_can_drag(tlistbox& list, ttoggle_panel& row);
	void did_visualize_tflite(ttoggle_panel& row);
	void click_set_test_script(tlistbox& list, ttoggle_panel& row);
	void click_lookover_tflite(tlistbox& list, ttoggle_panel& row);
	void click_upload_tflite(tlistbox& list, ttoggle_panel& row);
	void click_erase_tflite(tlistbox& list, ttoggle_panel& row);

	// service
	void pre_service();
	void click_showcase_model(tlistbox& list, ttoggle_panel& row);
	void click_download_network_item(tlistbox& list, ttoggle_panel& row);
	void click_erase_network_item(tgrid& layer, tlistbox& list, ttoggle_panel& row);
	void fill_items_list(tlistbox& list);
	int64_t calculate_available_space() const;
	void network_items_summary(tgrid& layer) const;
	bool did_network_can_drag(tlistbox& list, ttoggle_panel& row);
	void did_tflite_attrs(tlistbox& list, ttoggle_panel& row);
	void did_network_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer);

	// show
	void pre_showcase();
	enum {DISCOVER_LAYER};
	void pre_showcase_discover(tgrid& showcase_grid);
	void did_showcase_item_changed(treport& report, ttoggle_button& row);
	void fill_discover_tflites_list(tlistbox& list);
	void discover_tflites_summary(tgrid& layer) const;
	void click_follow_token_tflite(tgrid& layer, tlistbox& list, ttoggle_panel& row);
	void click_download_discover_tflite(tgrid& layer, tlistbox& list, ttoggle_panel& row);
	bool did_discover_tflite_can_drag(tlistbox& list, ttoggle_panel& row);
	void did_limit_followed_changed(tgrid& discover_grid, ttoggle_button& widget);
	void refresh_discover_tflites(tgrid& layer, int hidden_ms, const SDL_Rect& rect, bool force);
	void did_discocver_pullrefresh_refresh(ttrack& widget, const SDL_Rect& rect, tgrid& layer);

	// more
	void pre_more();
	void update_relative_login(tgrid& layer);
	void click_nickname(tgrid& layer);
	bool did_verify_nickname(const std::string& label);
	void click_login();
	void click_logout();
	void click_fullscreen(tgrid& layer);
	void click_resolution(tgrid& layer);
	void click_language(tgrid& layer);
	void click_member(tgrid& layer);
	void click_erase_user(tgrid& layer);
	void click_about(tgrid& layer);
	void handle_chat();

private:
	const config& app_cfg_;
	tscript& script_;
	int start_layer_;
	tstack* body_;
	tstack* showcase_stack_;
	ttoggle_button* limit_followed_;

	std::set<tscript> scripts_;
	bool scripts_dirty_;
	std::set<ttflite> tflites_;
	bool tflites_dirty_;
	std::set<std::string> local_images_;
	bool local_images_dirty_;

	std::set<tnetwork_item> network_items_;
	bool network_items_dirty_;

	std::set<ttoken_tflite> discover_tflites_;
	bool discover_tflites_received_;
	bool discover_tflites_dirty_;
};

} // namespace gui2

#endif

