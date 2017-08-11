#include "global.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "gui/dialogs/message.hpp"
#include "formula_string_utils.hpp"

#include "task.hpp"


const std::string& alias_2_path(const std::map<std::string, std::string>& paths, const std::string& alias)
{
	std::stringstream err;
	std::map<std::string, std::string>::const_iterator it = paths.find(alias);
	err << "Invalid alias: " << alias;
	VALIDATE(it != paths.end(), err.str());
	return it->second;
}

bool ttask::tsubtask::do_delete_path(const std::set<std::string>& paths) const
{
	utils::string_map symbols;
	for (std::set<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++ it) {
		const std::string& path = *it;
		if (!is_directory(path)) {
			continue;
		}
		symbols["name"] = path;
		if (!SDL_DeleteFiles(path.c_str())) {
#ifdef _WIN32
			const std::string errstr = SDL_GetError();
			DWORD err = GetLastError();
#endif
			gui2::show_message(null_str, vgettext2("Delete: $name fail!", symbols));
			return false;
		}
	}
	return true;
}

bool ttask::tsubtask::make_path(const std::string& alias, bool del) const
{
	utils::string_map symbols;
	const std::string& path = alias_2_path(alias);

	size_t pos = path.rfind("/");
	if (pos == std::string::npos) {
		return true;
	}

	std::string subpath = path.substr(0, pos);
	SDL_MakeDirectory(subpath.c_str());

	if (del) {
		if (!SDL_DeleteFiles(path.c_str())) {
			symbols["type"] = _("Directory");
			symbols["dst"] = path;
			gui2::show_message(null_str, vgettext2("Delete $type, from $dst fail!", symbols));
			return false;
		}
	}
	return true;
}

const std::string& ttask::tsubtask::alias_2_path(const std::string& alias) const
{
	return ::alias_2_path(paths_, alias);
}


std::string ttask::tsubtask::type_2_printable_str(const int type)
{
	if (type == ttask::res_file) {
		return _("File");
	} else if (type == ttask::res_dir) {
		return _("Directory");
	} else if (type == ttask::res_files) {
		return _("Files");
	}
	return null_str;
}

int ttask::tsubtask::str_2_type(const std::string& str)
{
	res_type type = res_none;
	if (str == "file") {
		type = res_file;
	} else if (str == "dir") {
		type = res_dir;
	} else if (str == "files") {
		type = res_files;
	}
	return type;
}

ttask::tsubtask_copy::tsubtask_copy(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& outer)
	: tsubtask(task, id, cfg, outer)
{
	std::vector<std::string> vstr2 = utils::split(cfg["function"].str());
	VALIDATE(vstr2.size() == 3, null_str);

	src_alias_ = vstr2[1];
	dst_alias_ = vstr2[2];
	// alias maybe set during runtime.
	// VALIDATE(!alias_2_path(src_alias_).empty() && !alias_2_path(dst_alias_).empty(), null_str); 

	vstr2 = utils::split(cfg["pre_remove"].str());
	// const std::string& dst_path = alias_2_path(dst_alias_);
	for (std::vector<std::string>::const_iterator it = vstr2.begin(); it != vstr2.end(); ++ it) {
		const std::string& subpath = *it;
		VALIDATE(!subpath.empty(), null_str);
		// app maybe modify value of dst_alias_, so save subpath.
		require_delete_.insert(subpath);
	}

	const config& op_cfg = cfg.child("resource");
	if (op_cfg) {
		BOOST_FOREACH (const config::attribute& attr, op_cfg.attribute_range()) {
			size_t pos = attr.first.find('-');
			VALIDATE(pos != std::string::npos, null_str);
			const std::string type_str = attr.first.substr(pos + 1);

			res_type type = res_none;
			if (type_str == "file") {
				type = res_file;
			} else if (type_str == "dir") {
				type = res_dir;
			} else if (type_str == "files") {
				type = res_files;
			}
			VALIDATE(type != res_none, "error resource type, must be file or dir!");

			std::vector<std::string> vstr = utils::split(attr.second);
			const int size = vstr.size();
			VALIDATE(size >= 1 && size <= 3, "resource item must be 1 or 2 or 3!");

			std::string name = vstr[0];
			std::replace(name.begin(), name.end(), '\\', '/');
			VALIDATE(name.back() != '/', "");

			std::string custom = size >= 2? vstr[1]: null_str;
			if (!custom.empty()) {
				std::replace(custom.begin(), custom.end(), '\\', '/');
				VALIDATE(custom.back() != '/', "");
			}

			const bool overwrite = vstr.size() <= 2? true: utils::to_bool(vstr[2], true);
			VALIDATE(type == res_file || overwrite, "overwirte = false only apply to file.");

			copy_res_.push_back(tres(type, name, custom, overwrite));
		}
	}
}

std::set<std::string> ttask::tsubtask_copy::generate_full_require_delete(const std::set<std::string>& require_delete)
{
	std::set<std::string> ret;
	const std::string& dst_path = alias_2_path(dst_alias_);
	for (std::set<std::string>::const_iterator it = require_delete_.begin(); it != require_delete_.end(); ++ it) {
		const std::string& subpath = *it;
		ret.insert(utils::normalize_path(dst_path + "/" + subpath));
	}
	return ret;
}

bool ttask::tsubtask_copy::handle()
{
	tcallback_lock lock(false, boost::bind(&ttask::tsubtask_copy::did_fail_rollback, this, _1));

	// 1. delete require deleted paths.
	if (!do_delete_path(generate_full_require_delete(require_delete_))) {
		return false;
	}

	if (!do_copy(src_alias_, dst_alias_)) {
		return false;
	}

	lock.set_result(true);
	return true;
}

bool ttask::tsubtask_copy::fail_rollback() const
{
	for (std::map<int, trollback>::const_iterator it = rollbacks_.begin(); it != rollbacks_.end(); ++ it) {
		const trollback& rollback = it->second;
		SDL_DeleteFiles(rollback.name.c_str()); // don't detect fail.
	}
	return true;
}

void ttask::tsubtask::replace_str(std::string& name, const std::pair<std::string, std::string>& replace) const
{
	if (replace.first.empty()) {
		return;
	}
	if (replace.first == replace.second) {
		return;
	}
	size_t pos = name.find(replace.first);
	while (pos != std::string::npos) {
		name.replace(pos, replace.first.size(), replace.second);
		pos = name.find(replace.first, pos + replace.second.size());
	}
}

bool ttask::tsubtask_copy::do_copy(const std::string& src_alias, const std::string& dst_alias)
{
	const std::string& src_path = alias_2_path(src_alias);
	const std::string& dst_path = alias_2_path(dst_alias);
	utils::string_map symbols;
	bool fok = true;
	std::string src, dst;
	std::vector<std::pair<std::string, std::string> > replaces = task_.app_get_replace(*this);

	current_number_ = 100000; // think one task cannot exceed this items.
	existed_paths_.clear();
	created_paths_.clear();
	rollbacks_.clear();

	// copy
	if (is_directory(src_path)) {
		for (std::vector<tres>::const_iterator it = copy_res_.begin(); it != copy_res_.end(); ++ it) {
			const tres r = *it;
			std::string actual_src_name = r.name;
			std::string actual_dst_name = r.custom.empty()? r.name: r.custom;

			for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
				const std::pair<std::string, std::string>& replace = *it;
				replace_str(actual_src_name, replace);				
				replace_str(actual_dst_name, replace);
			}

			src = utils::normalize_path(src_path + "/" + actual_src_name);
			dst = utils::normalize_path(dst_path + "/" + actual_dst_name);
			if (r.type == res_file) {
				if (!file_exists(src)) {
					continue;
				}
				if (!r.overwrite && file_exists(dst)) {
					continue;
				}
				if (!task_.app_filter(*this, src, false)) {
					continue;
				}

				resolve_res_2_rollback(r.type, dst);

				std::string tmp = dst.substr(0, dst.rfind('/'));
				SDL_MakeDirectory(tmp.c_str());

			} else if (r.type == res_dir || r.type == res_files) {
				if (!is_directory(src.c_str())) {
					continue;
				}
				if (!task_.app_filter(*this, src, true)) {
					continue;
				}
				if (r.type == res_dir) {
					resolve_res_2_rollback(r.type, dst);
					// make sure system don't exsit dst! FO_COPY requrie it.
					if (!SDL_DeleteFiles(dst.c_str())) {
						symbols["type"] = _("Directory");
						symbols["dst"] = dst;
						gui2::show_message(null_str, vgettext2("Delete $type, from $dst fail!", symbols));
						fok = false;
						break;
					}
				}
			}
			if (r.type == res_file || r.type == res_dir) {
				fok = SDL_CopyFiles(src.c_str(), dst.c_str());
				if (fok && r.type == res_dir) {
					fok = compare_directory(src, dst);
				}
			} else {
				bool has_resolved = false;
				if (!is_directory(dst)) {
					// it is necessary to create directory, think res_dir.
					resolve_res_2_rollback(res_dir, dst);
					has_resolved = true;
					SDL_MakeDirectory(dst.c_str());
				}
				std::set<std::string> files;
				fok = copy_root_files(src, dst, &files, boost::bind(&ttask::app_filter, &task_, *this, _1, false));
				if (!has_resolved) {
					resolve_res_2_rollback(r.type, dst, &files);
				}
			}
			if (!fok) {
				symbols["type"] = type_2_printable_str(r.type);
				symbols["src"] = src;
				symbols["dst"] = dst;
				gui2::show_message(null_str, vgettext2("Copy $type, from $src to $dst fail!", symbols));
				break;
			}
		}
	}

	return fok;
}

void ttask::tsubtask_copy::resolve_res_2_rollback(const res_type type, const std::string& name, const std::set<std::string>* files)
{
	std::string tmp;

	size_t pos = name.find('/'); // must exist.
	pos = name.find('/', pos + 1);

	while (pos != std::string::npos) {
		tmp = utils::lowercase(name.substr(0, pos));
		if (created_paths_.find(tmp) != created_paths_.end()) {
			// have required create it. do nothing.
			return;
		} else if (existed_paths_.find(tmp) != existed_paths_.end()) {
			// it is existe path, continue search more long path.
			pos = name.find('/', pos + 1);
			continue;
		}
		// decide to add existed_paths or created_paths
		if (is_directory(tmp)) {
			existed_paths_.insert(tmp);
		} else {
			// this time require create it. add a create rollback.
			created_paths_.insert(tmp);
			rollbacks_.insert(std::make_pair(current_number_ --, trollback(res_dir, tmp)));
			return;
		}
		pos = name.find('/', pos + 1);
	}
	// decide full name.
	if (type == res_dir) {
		// since copy directory, this directory must be destroy and create.
		created_paths_.insert(name);
		rollbacks_.insert(std::make_pair(current_number_ --, trollback(type, name)));

	} else if (type == res_files) {
		VALIDATE(is_directory(name), "do_copy must not res_files when directory doesn't exist.");
		for (std::set<std::string>::const_iterator it = files->begin(); it != files->end(); ++ it) {
			const std::string file = name + "/" + *it;
			rollbacks_.insert(std::make_pair(current_number_ --, trollback(res_file, file)));
		}

	} else {
		rollbacks_.insert(std::make_pair(current_number_ --, trollback(type, name)));
	}
}

void ttask::tsubtask_copy::did_fail_rollback(const bool result) const
{
	if (!result) {
		fail_rollback();
	}
}


ttask::tsubtask_remove::tsubtask_remove(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& outer)
	: tsubtask(task, id, cfg, outer)
{
	std::vector<std::string> vstr2 = utils::split(cfg["function"].str());
	VALIDATE(vstr2.size() == 2, null_str);

	obj_alias_ = vstr2[1];
	VALIDATE(!alias_2_path(obj_alias_).empty(), null_str); 

	const config& op_cfg = cfg.child("resource");
	if (op_cfg) {
		BOOST_FOREACH (const config::attribute& attr, op_cfg.attribute_range()) {
			size_t pos = attr.first.find('-');
			VALIDATE(pos != std::string::npos, null_str);
			const std::string type_str = attr.first.substr(pos + 1);

			res_type type = res_none;
			if (type_str == "file") {
				type = res_file;
			} else if (type_str == "dir") {
				type = res_dir;
			} else if (type_str == "files") {
				type = res_files;
			}
			VALIDATE(type != res_none, "error resource type, must be file or dir!");

			std::vector<std::string> vstr = utils::split(attr.second);
			VALIDATE(vstr.size() == 1, "resource item must be 2!");
			std::string name = vstr[0];
			std::replace(name.begin(), name.end(), '\\', '/');
			remove_res_.push_back(tres(type, name));
		}
	}
}

bool ttask::tsubtask_remove::handle()
{
	return do_remove(obj_alias_);
}

bool ttask::tsubtask_remove::do_remove(const std::string& alias) const
{
	const std::string& path = alias_2_path(alias);
	utils::string_map symbols;
	bool fok = true;
	std::string src, dst;
	std::vector<std::pair<std::string, std::string> > replaces = task_.app_get_replace(*this);

	// remove
	for (std::vector<tres>::const_iterator it = remove_res_.begin(); it != remove_res_.end(); ++ it) {
		const tres r = *it;
		std::string actual_name = r.name;

		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
			const std::pair<std::string, std::string>& replace = *it;
			replace_str(actual_name, replace);
		}
		dst = path + "/" + actual_name;
		if (r.type == res_file) {
			if (!file_exists(dst)) {
				continue;
			}
		} else {
			if (!is_directory(dst.c_str())) {
				continue;
			}
		}
		if (!SDL_DeleteFiles(dst.c_str())) {
			symbols["type"] = r.type == res_file? _("File"): _("Directory");
			symbols["dst"] = dst;
			gui2::show_message(null_str, vgettext2("Delete $type, from $dst fail!", symbols));
			fok = false;
			break;
		}
	}

	return fok;
}


ttask::tsubtask_rename::tsubtask_rename(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths)
	: tsubtask(task, id, cfg, base_paths)
{
	std::vector<std::string> vstr2 = utils::split(cfg["function"].str());
	VALIDATE(vstr2.size() == 2, null_str);

	base_path_alias_ = vstr2[1];
	VALIDATE(!alias_2_path(base_path_alias_).empty(), null_str); 

	const config& op_cfg = cfg.child("resource");
	if (op_cfg) {
		BOOST_FOREACH (const config::attribute& attr, op_cfg.attribute_range()) {
			size_t pos = attr.first.find('-');
			VALIDATE(pos != std::string::npos, null_str);
			const std::string type_str = attr.first.substr(pos + 1);

			res_type type = res_none;
			if (type_str == "file") {
				type = res_file;
			} else if (type_str == "dir") {
				type = res_dir;
			} else if (type_str == "files") {
				type = res_files;
			}
			VALIDATE(type != res_none, "error resource type, must be file or dir!");

			std::vector<std::string> vstr = utils::split(attr.second);
			VALIDATE(vstr.size() == 2, "resource item must be 2!");
			std::string name = vstr[0];
			std::replace(name.begin(), name.end(), '\\', '/');
			rename_res_.push_back(tres(type, name, vstr[1]));
		}
	}
}

bool ttask::tsubtask_rename::handle()
{
	return do_rename(base_path_alias_);
}

bool ttask::tsubtask_rename::do_rename(const std::string& alias) const
{
	const std::string& path = alias_2_path(alias);
	utils::string_map symbols;
	bool fok = true;
	std::string src, full_src;
	std::vector<std::pair<std::string, std::string> > replaces = task_.app_get_replace(*this);

	// rename
	for (std::vector<tres>::const_iterator it = rename_res_.begin(); it != rename_res_.end(); ++ it) {
		const tres r = *it;
		std::string actual_name = r.name;
		std::string actual_new_name = r.new_name;

		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
			const std::pair<std::string, std::string>& replace = *it;
			replace_str(actual_name, replace);
			replace_str(actual_new_name, replace);
		}
		full_src = path + "/" + actual_name;
		if (r.type == res_file) {
			if (!file_exists(full_src)) {
				continue;
			}
		} else {
			if (!is_directory(full_src.c_str())) {
				continue;
			}
		}
		if (!SDL_RenameFile(full_src.c_str(), actual_new_name.c_str())) {
			symbols["type"] = r.type == res_file? _("File"): _("Directory");
			symbols["src"] = full_src;
			symbols["dst"] = actual_new_name;
			gui2::show_message(null_str, vgettext2("Rename $type, from $src to $dst fail!", symbols));
			fok = false;
			break;
		}
	}

	return fok;
}


ttask::tsubtask_replace::tsubtask_replace(ttask& task, const std::string& id, const config& cfg, std::map<std::string, std::string>& base_paths)
	: tsubtask(task, id, cfg, base_paths)
{
	std::vector<std::string> vstr2 = utils::split(cfg["function"].str());
	VALIDATE(vstr2.size() == 2, null_str);

	base_path_alias_ = vstr2[1];
	VALIDATE(!alias_2_path(base_path_alias_).empty(), null_str); 

	const config& op_cfg = cfg.child("resource");
	if (op_cfg) {
		BOOST_FOREACH (const config::attribute& attr, op_cfg.attribute_range()) {
			size_t pos = attr.first.find('-');
			VALIDATE(pos != std::string::npos, null_str);
			const std::string type_str = attr.first.substr(pos + 1);

			int type = str_2_type(type_str);
			VALIDATE(type != res_none, "error resource type, must be file or dir!");

			std::vector<std::string> vstr = utils::split(attr.second);
			int size = vstr.size();
			VALIDATE(size >= 3 && (size & 0x1), "resource item' count must >= 3 and odd!");

			std::string file = vstr[0];
			std::replace(file.begin(), file.end(), '\\', '/');

			std::vector<std::pair<std::string, std::string> > replaces;
			for (int n = 1; n < size; n += 2) {
				replaces.push_back(std::make_pair(vstr[n], vstr[n + 1]));
			}
			replace_res_.push_back(tres(file, replaces));
		}
	}
}

bool ttask::tsubtask_replace::handle()
{
	const std::string& path = alias_2_path(base_path_alias_);
	utils::string_map symbols;
	bool fok = true;
	std::string src, full_src;
	std::vector<std::pair<std::string, std::string> > replaces = task_.app_get_replace(*this);

	// replace
	for (std::vector<tres>::const_iterator it = replace_res_.begin(); it != replace_res_.end(); ++ it) {
		const tres r = *it;
		std::string actual_file = r.file;
		std::vector<std::pair<std::string, std::string> > replaces2;

		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = replaces.begin(); it != replaces.end(); ++ it) {
			const std::pair<std::string, std::string>& replace = *it;
			replace_str(actual_file, replace);
		}
		
		std::string key, value;
		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = r.replaces.begin(); it != r.replaces.end(); ++ it) {
			key = it->first;
			value = it->second;
			for (std::vector<std::pair<std::string, std::string> >::const_iterator it2 = replaces.begin(); it2 != replaces.end(); ++ it2) {	
				const std::pair<std::string, std::string>& replace = *it2;
				replace_str(key, replace);
				replace_str(value, replace);
			}
			replaces2.push_back(std::make_pair(key, value));
		}

		full_src = path + "/" + actual_file;
		
		if (!file_exists(full_src)) {
			continue;
		}
		if (!file_replace_string(full_src, replaces2)) {
			symbols["file"] = full_src;
			gui2::show_message(null_str, vgettext2("Replace $file content fail!", symbols));
			fok = false;
			break;
		}
	}

	return fok;
}


const std::string ttask::res_alias = "res";
const std::string ttask::src_alias = "src";
const std::string ttask::src2_alias = "src2";
const std::string ttask::user_alias = "user";

const std::string ttask::app_res_alias = "app_res";
const std::string ttask::app_src_alias = "app_src";
const std::string ttask::app_src2_alias = "app_src2";

void ttask::complete_paths(const config& cfg)
{
	VALIDATE(paths_.empty(), null_str);

	// app can not define res/src2/src alias.
	// calculate src_path base on res_path
	const std::string res_path = game_config::path; // res_path always is game_config::path
	const std::string src2_path = extract_directory(res_path) + "/apps-src/apps";

	paths_.insert(std::make_pair(res_alias, res_path));
	paths_.insert(std::make_pair(src2_alias, src2_path));
	paths_.insert(std::make_pair(src_alias, extract_directory(src2_path)));
	paths_.insert(std::make_pair(user_alias, game_config::preferences_dir));

	// during app_complete_paths, can fill private alias, include app_res_alias, app_src2_alias, app_src_alias.
	app_complete_paths();

	// if app_complete_paths doesn't provide app_res_alias, default game_config::path to it.
	if (paths_.find(app_res_alias) == paths_.end()) {
		paths_.insert(std::make_pair(app_res_alias, game_config::path));
	}

	std::set<std::string> forbit_alias;
	forbit_alias.insert(res_alias);
	forbit_alias.insert(src_alias);
	forbit_alias.insert(src2_alias);
	forbit_alias.insert(user_alias);
	forbit_alias.insert(app_res_alias);
	forbit_alias.insert(app_src_alias);
	forbit_alias.insert(app_src2_alias);

	// alias in cfg cannot override forbit_alias.
	const std::string path_prefix = "alias-";
	BOOST_FOREACH (const config::attribute& attr, cfg.attribute_range()) {
		if (attr.first.find(path_prefix) != 0) {
			continue;
		}
		std::string alias = attr.first.substr(path_prefix.size());
		std::vector<std::string> vstr = utils::split(attr.second);
		
		VALIDATE(forbit_alias.find(alias) == forbit_alias.end(), null_str);
		VALIDATE(!alias.empty() && vstr.size() == 2, null_str);
		VALIDATE(paths_.find(alias) == paths_.end(), null_str);

		const std::string& base_path = alias_2_path(vstr[0]);
		paths_.insert(std::make_pair(alias, utils::normalize_path(base_path + "/" + vstr[1])));
	}
}

const std::string& ttask::alias_2_path(const std::string& alias) const
{
	return ::alias_2_path(paths_, alias);
}

void ttask::set_alias(const std::string& alias, const std::string& val)
{
	std::map<std::string, std::string>::iterator it = paths_.find(alias);
	if (it != paths_.end()) {
		if (it->second == val) {
			return;
		}
		paths_.erase(it);
	}
	paths_.insert(std::make_pair(alias, val));
}

ttask::ttask(const config&)
	: name_()
{}

void ttask::parse_cfg(const config& cfg, const std::string& key)
{
	std::vector<std::string> vstr = utils::split(cfg[key].str());
	if (vstr.empty()) {
		return;
	}
	complete_paths(cfg);

	const std::string function_id_copy = "copy";
	const std::string function_id_remove = "remove";
	const std::string function_id_rename = "rename";
	const std::string function_id_replace = "replace";

	for (std::vector<std::string>::const_iterator it = vstr.begin(); it != vstr.end(); ++ it) {
		const std::string& id = *it;
		const config& task_cfg = cfg.child(id);
		VALIDATE(task_cfg, std::string("Have not define subtask: ") + id);

		std::vector<std::string> vstr2 = utils::split(task_cfg["function"].str());
		VALIDATE(!vstr2.empty(), null_str);

		if (vstr2[0] == function_id_copy) {
			subtasks_.push_back(std::unique_ptr<tsubtask>(new tsubtask_copy(*this, id, task_cfg, paths_)));
		} else if (vstr2[0] == function_id_remove) {
			subtasks_.push_back(std::unique_ptr<tsubtask>(new tsubtask_remove(*this, id, task_cfg, paths_)));
		} else if (vstr2[0] == function_id_rename) {
			subtasks_.push_back(std::unique_ptr<tsubtask>(new tsubtask_rename(*this, id, task_cfg, paths_)));
		} else if (vstr2[0] == function_id_replace) {
			subtasks_.push_back(std::unique_ptr<tsubtask>(new tsubtask_replace(*this, id, task_cfg, paths_)));

		} else {
			std::stringstream err;
			err << "Unknown function " << vstr2[0] << ", in line: " << task_cfg["function"].str(); 
			VALIDATE(false, err.str());
		}
	}
}

bool ttask::handle()
{
	bool ret = true;
	int fail_rollback_from = 0;
	const int subtasks = subtasks_.size();
	for (std::vector<std::unique_ptr<tsubtask> >::const_iterator it = subtasks_.begin(); ret && it != subtasks_.end(); ++ it, fail_rollback_from ++) {
		tsubtask& subtask = **it;
		if (!app_can_execute(subtask, fail_rollback_from == subtasks - 1)) {
			continue;
		}
		ret = subtask.handle();
		if (ret) {
			ret = app_post_handle(subtask, fail_rollback_from == subtasks - 1);
		}
	}

	if (!ret) {
		if (fail_rollback_from) {
			// subtask result in fail, it's rollback is processed by handle.
			fail_rollback_from --;

		} else {
			// first subtask fail. rollback is processed by itself's handle.
			fail_rollback_from = nposm;
		}
	}

	if (!ret && fail_rollback_from != nposm) {
		for (int at = fail_rollback_from; at >= 0; at --) {
			const tsubtask& subtask = *(subtasks_[at]);
			if (!subtask.fail_rollback()) {
				break;
			}
		}
	}

	return ret;
}

void ttask::fail_rollback()
{
	// rollback all subtask.
	const int subtasks = subtasks_.size();
	VALIDATE(subtasks >= 0, null_str);

	int fail_rollback_from = subtasks - 1;
	for (int at = fail_rollback_from; at >= 0; at --) {
		const tsubtask& subtask = *(subtasks_[at]);
		if (!subtask.fail_rollback()) {
			break;
		}
	}
}

bool ttask::valid() const
{
	if (paths_.empty()) {
		return false;
	}
	for (std::map<std::string, std::string>::const_iterator it = paths_.begin(); it != paths_.end(); ++ it) {
		const std::string& path = it->second;
		if (path.size() < 2 || path.at(1) != ':') {
			return false;
		}
	}
	return true;
}

bool ttask::make_path(const std::string& tag, bool del) const
{
	utils::string_map symbols;
	const std::string& path = alias_2_path(tag);

	size_t pos = path.rfind("/");
	if (pos == std::string::npos) {
		return true;
	}

	std::string subpath = path.substr(0, pos);
	SDL_MakeDirectory(subpath.c_str());

	if (del) {
		if (!SDL_DeleteFiles(path.c_str())) {
			symbols["type"] = _("Directory");
			symbols["dst"] = path;
			gui2::show_message(null_str, vgettext2("Delete $type, from $dst fail!", symbols));
			return false;
		}
	}
	return true;
}