#ifndef OCR_DISPLAY_HPP_INCLUDED
#define OCR_DISPLAY_HPP_INCLUDED

#include "display.hpp"

class ocr_controller;
class ocr_unit_map;
class ocr_unit;

class ocr_display : public display
{
public:
	ocr_display(ocr_controller& controller, ocr_unit_map& units, CVideo& video, const tmap& map, int initial_zoom);
	~ocr_display();

	bool in_theme() const override { return true; }
	ocr_controller& get_controller() { return controller_; }
	
protected:
	void draw_sidebar();

private:
	gui2::tdialog* app_create_scene_dlg() override;
	void app_post_initialize() override;
	void app_post_set_zoom(int old_zoom) override;
	void app_draw_minimap_units(surface& screen) override;

private:
	ocr_controller& controller_;
	ocr_unit_map& units_;
};

#endif
