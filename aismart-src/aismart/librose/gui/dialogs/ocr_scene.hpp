#ifndef GUI_DIALOGS_OCR_THEME_HPP_INCLUDED
#define GUI_DIALOGS_OCR_THEME_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class ocr_controller;

namespace gui2 {

class ttoggle_panel;

class tocr_scene: public tdialog
{
public:
	enum { ZOOM, POSITION, NUM_REPORTS};

	enum {
		HOTKEY_RETURN = HOTKEY_MIN,
		HOTKEY_RECOGNIZE,
	};

	tocr_scene(ocr_controller& controller);

	void fill_fields_list();

private:
	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	const std::string& window_id() const override;

	void pre_show() override;

	void signal_handler_longpress_name(bool& halt, const tpoint& coordinate, ttoggle_panel& row);

private:
	ocr_controller& controller_;
};

} //end namespace gui2

#endif
