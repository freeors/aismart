#ifndef GUI_DIALOGS_USER_MANAGER_HPP_INCLUDED
#define GUI_DIALOGS_USER_MANAGER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class ttree_node;

class tuser_manager: public tdialog
{
public:
	explicit tuser_manager(std::map<std::string, std::string>& private_keys);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	enum {type_none, type_png, type_script, type_tflite};
	void set_remark_label(bool hit);

	ttree_node& add_explorer_node(const std::string& dir, ttree_node& parent, const std::string& name, bool isdir);
	bool compare_sort(const ttree_node& a, const ttree_node& b);
	bool walk_dir2(const std::string& dir, const SDL_dirent2* dirent, ttree_node* root);
	void refresh_explorer_tree();
	void did_explorer_folded(ttree_node& node, const bool folded);

	void signal_handler_longpress_name(bool& halt, const tpoint& coordinate, ttree_node& node);
	bool did_drag_mouse_motion(const int x, const int y);
	void did_drag_mouse_leave(const int x, const int y, bool up_result);

	struct tcookie {
		tcookie(const std::string& dir, const std::string& name, bool isdir)
			: dir(dir)
			, name(name)
			, isdir(isdir)
			, type(type_none)
		{}

		const std::string dir;
		const std::string name;
		bool isdir;
		int type;
	};
	int judge_type(const tcookie& cookie) const;
	void handle_copy(const tcookie& cookie) const;

private:
	std::map<std::string, std::string>& private_keys_;
	std::map<const ttree_node*, tcookie> cookies_;

	const tcookie* dragging_cookie_;
};

} // namespace gui2

#endif

