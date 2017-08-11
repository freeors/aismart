#ifndef GUI_DIALOGS_TFLITE_ATTRS_HPP_INCLUDED
#define GUI_DIALOGS_TFLITE_ATTRS_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "game_config.hpp"


namespace gui2 {

class ttoggle_button;

class ttflite_attrs: public tdialog
{
public:
	explicit ttflite_attrs(tnetwork_item& item);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	bool did_verify_name(const std::string& label);
	void click_name();
	void click_state();
	void click_keys();

	bool did_keyword_selector_changed(ttoggle_button& widget, const int at, const std::vector<bool>& states);

private:
	tnetwork_item& item_;
};

} // namespace gui2

#endif

