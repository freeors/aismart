#ifndef GUI_DIALOGS_BROWSE_SCRIPT_HPP_INCLUDED
#define GUI_DIALOGS_BROWSE_SCRIPT_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "game_config.hpp"

namespace gui2 {

class ttoggle_button;

class tbrowse_script: public tdialog
{
public:
	explicit tbrowse_script(const std::string& title, const std::string& label, const std::string& cancel_label, const std::string& ok_label);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void did_agree_changed(ttoggle_button& widget);

private:
	const std::string title_;
	const std::string label_;
	const std::string cancel_label_;
	const std::string ok_label_;
};

} // namespace gui2

#endif

