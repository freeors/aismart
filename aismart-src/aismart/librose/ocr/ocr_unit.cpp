#define GETTEXT_DOMAIN "smsrobot-lib"

#include "ocr_unit.hpp"
#include "gettext.hpp"
#include "ocr_display.hpp"
#include "ocr_controller.hpp"
#include "gui/dialogs/ocr_scene.hpp"

ocr_unit::ocr_unit(ocr_controller& controller, ocr_display& disp, ocr_unit_map& units, const tocr_line& line)
	: base_unit(units)
	, controller_(controller)
	, disp_(disp)
	, units_(units)
	, line_(line)
{
	set_rect(line_.bounding_rect);
}

void ocr_unit::app_draw_unit(const int xsrc, const int ysrc)
{
	Uint32 char_color = 0xffff0000;

	double zoom_factor = disp_.get_zoom_factor();
	for (std::map<trect, int>::const_iterator it = line_.chars.begin(); it != line_.chars.end(); ++ it) {
		const trect& rect = it->first;

		if (it->second != 256) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_FRAME, 
				xsrc + (rect.x * zoom_factor - rect_.x), 
				ysrc + (rect.y * zoom_factor - rect_.y), 
				rect.w * zoom_factor, 
				rect.h * zoom_factor, 
				char_color);
		} else {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_RECT, 
				xsrc + (rect.x * zoom_factor - rect_.x), 
				ysrc + (rect.y * zoom_factor - rect_.y), 
				rect.w * zoom_factor, 
				rect.h * zoom_factor, 
				0x80ff0000);
		}
	}

	if (!line_.field.empty()) {
		surface field_surf = font::get_rendered_text(line_.field, 0, font::SIZE_SMALLER, font::BLUE_COLOR);
		disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, field_surf, xsrc, ysrc, 0, 0);
	}

	// red: drag hit
	// green: bound
	Uint32 bounding_color = line_.field.empty()? 0xffffffff: 0xff00ff00;
	if (controller_.dragging_field() != nposm) {
		int xmouse, ymouse;
		SDL_GetMouseState(&xmouse, &ymouse);
		SDL_Rect hex_rect {xsrc, ysrc, rect_.w, rect_.h};
		SDL_Rect intersect;
		SDL_IntersectRect(&hex_rect, &disp_.main_map_widget_rect(), &intersect);
		if (point_in_rect(xmouse, ymouse, intersect)) {
			bounding_color = (redraw_counter_ / 10) & 0x1? 0xff000000: 0xffffffff;
		}
	}

	disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_FRAME, 
		xsrc, ysrc, rect_.w, rect_.h, bounding_color);

	// last add adjusting_line
	const std::pair<ocr_unit*, int> adjusting_line = controller_.adjusting_line();
	if (adjusting_line.first == this) {
		Uint32 adjusting_line_color = 0xffff0000;
		if (adjusting_line.second == base_controller::UP) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_LINE, 
				xsrc, ysrc, xsrc + rect_.w - 1, ysrc, adjusting_line_color);

		} else if (adjusting_line.second == base_controller::DOWN) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_LINE, 
				xsrc, ysrc + rect_.h - 1, xsrc + rect_.w - 1, ysrc + rect_.h - 1, adjusting_line_color);

		} else if (adjusting_line.second == base_controller::LEFT) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_LINE, 
				xsrc, ysrc, xsrc, ysrc + rect_.h - 1, adjusting_line_color);

		} else if (adjusting_line.second == base_controller::RIGHT) {
			disp_.drawing_buffer_add(display::LAYER_UNIT_DEFAULT, loc_, image::BLITM_LINE, 
				xsrc + rect_.w - 1, ysrc, xsrc + rect_.w - 1, ysrc + rect_.h - 1, adjusting_line_color);
		}
	}
}