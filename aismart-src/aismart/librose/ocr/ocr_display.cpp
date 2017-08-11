#define GETTEXT_DOMAIN "smsrobot-lib"

#include "ocr_display.hpp"
#include "ocr_controller.hpp"
#include "ocr_unit_map.hpp"
#include "gui/dialogs/ocr_scene.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "halo.hpp"
#include "formula_string_utils.hpp"

ocr_display::ocr_display(ocr_controller& controller, ocr_unit_map& units, CVideo& video, const tmap& map, int initial_zoom)
	: display(game_config::tile_square, controller, video, &map, gui2::tocr_scene::NUM_REPORTS, initial_zoom)
	, controller_(controller)
	, units_(units)
{
	min_zoom_ = 64;
	max_zoom_ = 1024;

	show_hover_over_ = false;
	set_grid(true);
}

ocr_display::~ocr_display()
{
}

gui2::tdialog* ocr_display::app_create_scene_dlg()
{
	return new gui2::tocr_scene(controller_);
}

void ocr_display::app_post_initialize()
{
}

void ocr_display::draw_sidebar()
{
	// Fill in the terrain report
	if (map_->on_board_with_border(mouseoverHex_)) {
		refresh_report(gui2::tocr_scene::POSITION, reports::report(reports::report::LABEL, lexical_cast<std::string>(mouseoverHex_), null_str));
	}
	std::stringstream ss;
	ss << zoom_ << "(" << int(get_zoom_factor() * 100) << "%)";
	refresh_report(gui2::tocr_scene::ZOOM, reports::report(reports::report::LABEL, ss.str(), null_str));
}

void ocr_display::app_post_set_zoom(int old_zoom)
{
	double zoom_factor = get_zoom_factor();

	int size = units_.size();
	for (int at = 0; at < size; at ++) {
		ocr_unit* u = units_.find_unit(at);
		const SDL_Rect& bounding_rect = u->line().bounding_rect;
		u->set_rect(create_rect(bounding_rect.x * zoom_factor, bounding_rect.y * zoom_factor, bounding_rect.w * zoom_factor, bounding_rect.h * zoom_factor));
	}
}

void ocr_display::app_draw_minimap_units(surface& screen)
{
	double xscaling = 1.0 * minimap_location_.w / map_->w();
	double yscaling = 1.0 * minimap_location_.h / map_->h();

	std::vector<SDL_Rect> rects;
	xscaling = 1.0 * minimap_location_.w / (map_->w() * hex_width());
	yscaling = 1.0 * minimap_location_.h / (map_->h() * hex_width());

	for (int at = 0; at < units_.size(); at ++) {
		const ocr_unit* u = dynamic_cast<const ocr_unit*>(units_.find_unit(at));
		const tocr_line& line = u->line();

		const SDL_Rect& rect = u->get_rect();
		double u_x = rect.x * xscaling;
		double u_y = rect.y * yscaling;
		double u_w = rect.w * xscaling;
		double u_h = rect.h * yscaling;
		
		const Uint32 mapped_col = 0xffff0000;

		draw_rectangle(minimap_location_.x + round_double(u_x)
			, minimap_location_.y + round_double(u_y)
			, round_double(u_w)
			, round_double(u_h)
			, mapped_col, screen);

		if (!line.field.empty()) {
			surface field_surf = font::get_rendered_text(line.field, 0, font::SIZE_SMALLEST, font::BLUE_COLOR);
			SDL_Rect dstrect {minimap_location_.x + round_double(u_x), minimap_location_.y + round_double(u_y), field_surf->w, field_surf->h};
			sdl_blit(field_surf, nullptr, screen, &dstrect);
		}
	}
}