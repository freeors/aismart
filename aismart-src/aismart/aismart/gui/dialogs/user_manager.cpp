#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/user_manager.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "filesystem.hpp"
#include "font.hpp"
#include "formula_string_utils.hpp"
#include "tflite.hpp"

#include <boost/bind.hpp>


namespace gui2 {

REGISTER_DIALOG(aismart, user_manager)

tuser_manager::tuser_manager(std::map<std::string, std::string>& private_keys)
	: private_keys_(private_keys)
	, dragging_cookie_(nullptr)
{
}

void tuser_manager::pre_show()
{
	window_->set_label("misc/white-background.png");

	find_widget<tbutton>(window_, "ok", false).set_icon("misc/back.png");

	set_remark_label(false);

	ttree* explorer = find_widget<ttree>(window_, "explorer", false, true);
	explorer->enable_select(false);
	explorer->set_did_fold_changed(boost::bind(&tuser_manager::did_explorer_folded, this, _2, _3));

	refresh_explorer_tree();
}

void tuser_manager::post_show()
{
}

int tuser_manager::judge_type(const tcookie& cookie) const
{
	if (cookie.isdir) {
		return type_none;
	}

	if (cookie.dir.find(game_config::preferences_dir) == 0) {
		return type_none;
	}

	std::map<int, std::string> types;
	types.insert(std::make_pair(type_png, ".png"));
	types.insert(std::make_pair(type_script, ".cfg"));
	types.insert(std::make_pair(type_tflite, ".tflite"));

	for (std::map<int, std::string>::const_iterator it = types.begin(); it != types.end(); ++ it) {
		const std::string& ext = it->second;
		size_t pos = cookie.name.rfind(ext);
		if (pos != std::string::npos && pos + ext.size() == cookie.name.size()) {
			if (it->first == type_png) {
				surface surf = image::get_image(cookie.dir + "/" + cookie.name);
				if (surf.get()) {
					return it->first;
				}
				continue;

			} else if (it->first != type_tflite) {
				return it->first;
			}
			std::string file = cookie.dir + "/" + cookie.name;
			if (!tflite::is_valid_tflite(file)) {
				continue;
			}
			file = cookie.dir + "/" + tflite::labels_txt;
			if (SDL_IsFile(file.c_str())) {
				return it->first;
			}
		}
	}
	return type_none;
}

void tuser_manager::set_remark_label(bool hit)
{
	tlabel& label = find_widget<tlabel>(window_, "remark", false);

	std::stringstream ss;
	if (!dragging_cookie_) {
		VALIDATE(!hit, null_str);
		ss << _("Long press *.tflite(require labels.txt), *.png or *.cfg, and drag it to right 'Docuemnt' icon.");

	} else {
		if (!hit) {
			ss << _("Drag it to right 'Document' icon.");
		} else {

			::utils::string_map symbols;

			if (dragging_cookie_->type != type_tflite) {
				symbols["files"] = dragging_cookie_->name;

			} else {
				symbols["files"] = dragging_cookie_->name + ", labels.txt";
			}
			ss << vgettext2("Release will copy $files to userdata.", symbols);
		}

	}
	label.set_label(ss.str());
}

ttree_node& tuser_manager::add_explorer_node(const std::string& dir, ttree_node& parent, const std::string& name, bool isdir)
{
	std::map<std::string, std::string> tree_group_item;

	tree_group_item["text"] = name;
	ttree_node& htvi = parent.insert_node("default", tree_group_item, nposm, isdir);
	htvi.set_child_icon("text", isdir? "misc/folder.png": "misc/file.png");

	cookies_.insert(std::make_pair(&htvi, tcookie(dir, name, isdir)));

	htvi.connect_signal<event::LONGPRESS>(
			boost::bind(
				&tuser_manager::signal_handler_longpress_name
				, this
				, _4, _5, boost::ref(htvi))
			, event::tdispatcher::back_child);

	htvi.connect_signal<event::LONGPRESS>(
			boost::bind(
				&tuser_manager::signal_handler_longpress_name
				, this
				, _4, _5, boost::ref(htvi))
			, event::tdispatcher::back_post_child);

	return htvi;
}

bool tuser_manager::compare_sort(const ttree_node& a, const ttree_node& b)
{
	const tcookie& a2 = cookies_.find(&a)->second;
	const tcookie& b2 = cookies_.find(&b)->second;

	if (a2.isdir && !b2.isdir) {
		// tvi1 is directory, tvi2 is file
		return true;
	} else if (!a2.isdir && b2.isdir) {
		// tvi1 is file, tvi2 if directory
		return false;
	} else {
		// both lvi1 and lvi2 are directory or file, use compare string.
		return SDL_strcasecmp(a2.name.c_str(), b2.name.c_str()) <= 0? true: false;
	}
}

bool tuser_manager::walk_dir2(const std::string& dir, const SDL_dirent2* dirent, ttree_node* root)
{
	bool isdir = SDL_DIRENT_DIR(dirent->mode);
	add_explorer_node(dir, *root, dirent->name, isdir);

	return true;
}

void tuser_manager::refresh_explorer_tree()
{
	cookies_.clear();

	ttree* explorer = find_widget<ttree>(window_, "explorer", false, true);
	ttree_node& root = explorer->get_root_node();
	explorer->clear();
	VALIDATE(explorer->empty(), null_str);

	std::vector<std::string> disks;
	if (!game_config::mobile) {
		disks = get_disks();
	} else {
		disks.clear();
		disks.push_back(game_config::path);
	}
	for (std::vector<std::string>::const_iterator it = disks.begin(); it != disks.end(); ++ it) {
		const std::string& disk = *it;
		ttree_node& htvi = add_explorer_node(disk, root, disk == game_config::path? "<res>": disk, true);

		::walk_dir(disk, false, boost::bind(
				&tuser_manager::walk_dir2
				, this
				, _1, _2, &htvi));

		htvi.sort_children(boost::bind(&tuser_manager::compare_sort, this, _1, _2));
		if (disks.size() == 1) {
			htvi.unfold();
		}
	}
}

void tuser_manager::did_explorer_folded(ttree_node& node, const bool folded)
{
	if (!folded && node.empty()) {
		const tcookie& cookie = cookies_.find(&node)->second;
		// when dir is first, it will end width '/'. for example root directory on linux.
		std::string dir = cookie.dir + '/' + cookie.name;

		dir = utils::normalize_path(dir);

		::walk_dir(dir, false, boost::bind(
				&tuser_manager::walk_dir2
				, this
				, _1, _2, &node));
		node.sort_children(boost::bind(&tuser_manager::compare_sort, this, _1, _2));
	}
}

void tuser_manager::signal_handler_longpress_name(bool& halt, const tpoint& coordinate, ttree_node& node)
{
	halt = true;

	tcookie& cookie = cookies_.find(&node)->second;
	cookie.type = judge_type(cookie);
	if (cookie.type == type_none) {
		return;
	}

	surface surf = font::get_rendered_text(cookie.name, 0, font::SIZE_LARGEST + 2 * twidget::hdpi_scale, font::BLACK_COLOR);
	window_->set_drag_surface(surf, true);

	dragging_cookie_ = &cookie;

	set_remark_label(false);

	window_->start_drag(coordinate, boost::bind(&tuser_manager::did_drag_mouse_motion, this, _1, _2),
		boost::bind(&tuser_manager::did_drag_mouse_leave, this, _1, _2, _3));
}

bool tuser_manager::did_drag_mouse_motion(const int x, const int y)
{
	VALIDATE(dragging_cookie_, null_str);

	tcontrol* icon = find_widget<tcontrol>(window_, "userdata", false, true);
	bool hit = point_in_rect(x, y, icon->get_rect());
	set_remark_label(hit);

	return true;
}

/*
struct tkey_cfg
{
	static void create(const std::string& path, const std::string& key, const std::string& app_secret, const std::string& md);
};

void tkey_cfg::create(const std::string& path, const std::string& key, const std::string& app_secret, const std::string& md)
{
	VALIDATE(key.size() == 16 && app_secret.size() == 32 && md.size() == SHA256_DIGEST_LENGTH * 2, null_str);

	std::stringstream fp_ss;

	fp_ss << "#\n";
	fp_ss << "# NOTE: it is generated by aismart.\n";
	fp_ss << "#\n";
	fp_ss << "\n";

	const std::string aismart_key = "abcdefghijklmnop";
	if (key == aismart_key) {
		fp_ss << "key=\"" << key << "\"\n";
	}
	fp_ss << "app_secret=\"" << app_secret << "\"\n";
	fp_ss << "sha256=\"" << md << "\"";

	tfile file(path, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file.fp, fp_ss.str().c_str(), fp_ss.str().size());
}
*/
void tuser_manager::handle_copy(const tcookie& cookie) const
{
	const std::string src = cookie.dir + '/' + cookie.name;

	VALIDATE(cookie.type != type_none, null_str);

	std::string dst_dir;
	if (cookie.type == type_png) {
		dst_dir = game_config::preferences_dir + "/images/misc";

	} else if (cookie.type == type_script) {
		dst_dir = game_config::preferences_dir + "/tflites";

	} else {
		VALIDATE(cookie.type == type_tflite, null_str);
		size_t pos = cookie.name.rfind('.');
		dst_dir = game_config::preferences_dir + "/tflites/" + cookie.name.substr(0, pos);
	}
	
	std::string dst = dst_dir + "/" + cookie.name;
	if (cookie.type == type_tflite) {
		// *.tflite ---> *.tflits
		dst[dst.size() - 1] = 's';
	}
	if (SDL_IsFile(dst.c_str())) {
		::utils::string_map symbols;
		symbols["file"] = cookie.name;
		const std::string tip = vgettext2("$file is existed in userdata, do you want to overwirte it?", symbols);
		int res = gui2::show_message("", tip, gui2::tmessage::yes_no_buttons);
		if (res != gui2::twindow::OK) {
			return;
		}

	}
	
	SDL_MakeDirectory(dst_dir.c_str());
	if (cookie.type != type_tflite) {
		SDL_CopyFiles(src.c_str(), dst.c_str());
	} else {
		size_t pos = cookie.name.rfind('.');
		const std::string main_name = cookie.name.substr(0, pos);

		tflite::encrypt_tflite(src, dst, game_config::showcase_sha256);

		const std::string labels_txt_src = cookie.dir + '/' + tflite::labels_txt;
		SDL_CopyFiles(labels_txt_src.c_str(), (dst_dir + "/" + tflite::labels_txt).c_str());
	}
}

void tuser_manager::did_drag_mouse_leave(const int x, const int y, bool up_result)
{
	VALIDATE(dragging_cookie_, null_str);
	if (up_result) {
		tcontrol* icon = find_widget<tcontrol>(window_, "userdata", false, true);
		if (point_in_rect(x, y, icon->get_rect())) {
			handle_copy(*dragging_cookie_);
		}
	}

	dragging_cookie_ = nullptr;
	set_remark_label(false);
}

} // namespace gui2

