#define GETTEXT_DOMAIN "smsrobot-lib"

/*
 * How to use...
 * ocr_controller controller(game.app_cfg(), game.video());
 * controller.initialize();
 * int ret = controller.main_loop();
 */

#include "ocr_controller.hpp"
#include "ocr_display.hpp"
#include "gui/dialogs/ocr_scene.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/widgets/window.hpp"
#include "rose_config.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"

#include <opencv2/imgproc.hpp>

#include "tflite.hpp"

#include <boost/bind.hpp>

ocr_controller::ocr_controller(const config& app_cfg, CVideo& video, surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines, const std::vector<std::string>& fields, const std::string& tflite, const std::string& sha256, const tflite::tparams& tflite_params)
	: base_controller(SDL_GetTicks(), app_cfg, video)
	, gui_(nullptr)
	, map_(null_str)
	, units_(*this, map_, false)
	, target_(surf)
	, lines_(lines)
	, fields_(fields)
	, pb_path_(get_binary_file_location("tflites", tflite + "/" + tflite + ".tflits"))
	, sha256_(sha256)
	, tflite_params_(tflite_params)
	, dragging_field_(nposm)
	, adjusting_line_(std::make_pair(nullptr, 0))
	, start_adjusting_xy_(construct_null_coordinate())
{
	map_ = tmap(surf, 72);

	units_.create_coor_map(map_.w(), map_.h());
}

ocr_controller::~ocr_controller()
{
	if (gui_) {
		delete gui_;
		gui_ = nullptr;
	}
}

void ocr_controller::app_create_display(int initial_zoom)
{
	gui_ = new ocr_display(*this, units_, video_, map_, initial_zoom);
}

void ocr_controller::app_post_initialize()
{
	std::vector<cv::Scalar> line_colors;
	line_colors.push_back(cv::Scalar(0, 0, 0, 255));
	line_colors.push_back(cv::Scalar(255, 255, 255, 255));
	line_colors.push_back(cv::Scalar(0, 0, 255, 255));
	line_colors.push_back(cv::Scalar(0, 255, 0, 255));
	line_colors.push_back(cv::Scalar(255, 0, 0, 255));
	line_colors.push_back(cv::Scalar(0, 255, 255, 255));
	line_colors.push_back(cv::Scalar(255, 255, 0, 255));
	line_colors.push_back(cv::Scalar(255, 0, 255, 255));
	line_colors.push_back(cv::Scalar(119, 119, 119, 255));
	line_colors.push_back(cv::Scalar(48, 59, 255, 255));
	line_colors.push_back(cv::Scalar(105, 215, 83, 255));
	line_colors.push_back(cv::Scalar(0, 128, 128, 255));
	line_colors.push_back(cv::Scalar(128, 128, 0, 255));
	line_colors.push_back(cv::Scalar(128, 0, 128, 255));

	int at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines_.begin(); it != lines_.end(); ++ it, at ++) {
		const tocr_line& line = *it->get();
		ocr_unit* u = new ocr_unit(*this, *gui_, units_, line);
		units_.insert2(*gui_, u);
	}

	bind_field_2_unit(0, *units_.find_unit(0));
}

void ocr_controller::app_execute_command(int command, const std::string& sparam)
{
	using namespace gui2;

	switch (command) {
		case tocr_scene::HOTKEY_RETURN:
			do_quit_ = true;
			quit_mode_ = gui2::twindow::CANCEL;
			break;

		case tocr_scene::HOTKEY_RECOGNIZE:
			recognize();
			break;

		case HOTKEY_ZOOM_IN:
			gui_->set_zoom(gui_->zoom());
			break;
		case HOTKEY_ZOOM_OUT:
			gui_->set_zoom(- gui_->zoom() / 2);
			break;

		case HOTKEY_SYSTEM:
			break;

		default:
			base_controller::app_execute_command(command, sparam);
	}
}

bool ocr_controller::app_mouse_motion(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return true;
	}

	if (adjusting_line_.first) {
		int xmap = x, ymap = y;
		gui_->screen_2_map(xmap, ymap);
		const SDL_Rect map_rect = gui_->main_map_rect();
		SDL_Rect rect = adjusting_line_.first->get_rect();
		if (adjusting_line_.second == UP) {
			if (ymap < 0) {
				ymap = 0;
			}
			rect.h += rect.y - ymap;
			rect.y = ymap;
			
		} else if (adjusting_line_.second == DOWN) {
			if (ymap > map_rect.h) {
				ymap = map_rect.h;
			}
			rect.h = ymap - rect.y;

		} else if (adjusting_line_.second == LEFT) {
			if (xmap < 0) {
				xmap = 0;
			}
			rect.w += rect.x - xmap;
			rect.x = xmap;
			
		} else if (adjusting_line_.second == RIGHT) {
			if (xmap > map_rect.w) {
				xmap = map_rect.w;
			}
			rect.w = xmap - rect.x;
		} 	

		if (!SDL_RectEmpty(&rect)) {
			adjusting_line_.first->set_rect(rect);
		}
	}

	return adjusting_line_.first? false: true;
}

void ocr_controller::app_left_mouse_down(const int x, const int y, const bool minimap)
{
	if (minimap) {
		return;
	}

	const map_location& mouseover_hex = gui_->mouseover_hex();
	if (!map_.on_board(mouseover_hex)) {
		return;
	}

	{
		int ii = 0;
		return;
	}

	VALIDATE(!adjusting_line_.first, null_str);

	const int cursor_radius = 18 * gui2::twidget::hdpi_scale;
	int xmap = x, ymap = y;
	gui_->screen_2_map(xmap, ymap);

	// in order to can select vertical, don't extern horizontal.
	SDL_Rect horizontal_cursor_rect{xmap, ymap - cursor_radius, 1, cursor_radius * 2};
	SDL_Rect virtical_cursor_rect{xmap - cursor_radius, ymap, cursor_radius * 2, 1};

	const int size = units_.size();
	for (int at = 0; at < size; at ++) {
		ocr_unit* u = units_.find_unit(at);

		int edge = -1;
		const SDL_Rect& rect = u->get_rect();
		// leave left and right edge to can select vertical.
		SDL_Rect edge_rect = create_rect(rect.x + cursor_radius, rect.y, rect.w - cursor_radius * 2, 1);
		if (edge_rect.w < cursor_radius) {
			// it is very small in w, use original x and w.
			edge_rect.x = rect.x;
			edge_rect.w = rect.w;
		}
		if (rects_overlap(edge_rect, horizontal_cursor_rect)) {
			edge = UP;
		}
		if (edge == -1) {
			// leave left and right edge to can select vertical.
			edge_rect = create_rect(rect.x + cursor_radius, rect.y + rect.h - 1, rect.w - cursor_radius * 2, 1);
			if (edge_rect.w < cursor_radius) {
				// it is very small in w, use original x and w.
				edge_rect.x = rect.x;
				edge_rect.w = rect.w;
			}
			if (rects_overlap(edge_rect, horizontal_cursor_rect)) {
				edge = DOWN;
			}
		}

		if (edge == -1) {
			edge_rect = create_rect(rect.x, rect.y, 1, rect.h);
			if (rects_overlap(edge_rect, virtical_cursor_rect)) {
				edge = LEFT;
			}
		}
		if (edge == -1) {
			edge_rect = create_rect(rect.x + rect.w - 1, rect.y, 1, rect.h);
			if (rects_overlap(edge_rect, virtical_cursor_rect)) {
				edge = RIGHT;
			}
		}
		if (edge != -1) {
			adjusting_line_ = std::make_pair(u, edge);
			start_adjusting_xy_ = tpoint(x, y);
		}
	}
}

void ocr_controller::app_mouse_leave_main_map(const int x, const int y, const bool minimap)
{
	adjusting_line_.first = nullptr;
}

bool ocr_controller::did_drag_mouse_motion(const int x, const int y, gui2::twindow& window)
{
	VALIDATE(dragging_field_ != nposm, null_str);
	const base_unit* u = units_.find_base_unit(x, y);

	
	std::string border = "label-tooltip";
	gui2::tfloat_widget& drag_widget = window.drag_widget();
	if (u) {
		border = "float_widget";
	}
	// drag_widget.widget->set_border(border);

	return true;
}

void ocr_controller::did_drag_mouse_leave(const int x, const int y, bool up_result)
{
	VALIDATE(dragging_field_ != nposm, null_str);
	if (up_result) {
		base_unit* u = units_.find_base_unit(x, y);
		if (u) {
			bind_field_2_unit(dragging_field_, *dynamic_cast<ocr_unit*>(u));
		}
	}
	dragging_field_ = nposm;
}

void ocr_controller::bind_field_2_unit(int dragging_field_, ocr_unit& to_unit)
{
	VALIDATE(dragging_field_ != nposm, null_str);
	const std::string& hit_field = fields_[dragging_field_];

	tocr_line& to_line = to_unit.line();
	if (to_line.field == hit_field) {
		return;
	}

	// clear field if other line bound it.
	bool cleared = false;
	const int size = units_.size();
	for (int at = 0; at < size; at ++) {
		tocr_line& line = units_.find_unit(at)->line();
		if (line.field == hit_field) {
			VALIDATE(!cleared, null_str);
			line.field.clear();
			cleared = true;
		}
	}

	// bind file to this line.
	to_line.field = hit_field;

	gui2::tocr_scene* scene = dynamic_cast<gui2::tocr_scene*>(gui_->get_theme());
	scene->fill_fields_list();

	gui_->redraw_minimap();

	dynamic_cast<gui2::tbutton*>(gui_->get_theme_object("recognize"))->set_active(true);
}

void ocr_controller::recognize()
{
	recognize_result_.clear();

	gui2::run_with_progress(null_str, boost::bind(&ocr_controller::did_recognize, this, _1), true, 0, false);

	// gui2::tocr_scene* scene = dynamic_cast<gui2::tocr_scene*>(gui_->get_theme());
	// scene->fill_fields_list();

	do_quit_ = true;
}

bool ocr_controller::did_recognize(gui2::tprogress_& progress)
{
	tsurface_2_mat_lock lock(target_);
	cv::Mat gray;
	cv::cvtColor(lock.mat, gray, cv::COLOR_BGRA2GRAY);

	std::string result;
	int size = units_.size();

	int total_chars = 0, recognized_chars = 0;
	for (int at = 0; at < size; at ++) {
		ocr_unit* u = units_.find_unit(at);
		const tocr_line& line = u->line();
		if (line.field.empty()) {
			continue;
		}
		total_chars += line.chars.size();
	}

	utils::string_map symbols;

	for (int at = 0; at < size; at ++) {
		ocr_unit* u = units_.find_unit(at);
		const tocr_line& line = u->line();
		if (line.field.empty()) {
			continue;
		}
		result.clear();

		int char_at = 0;
		uint32_t used_ticks = 0;

		symbols["field"] = line.field;
		progress.set_message(vgettext2("Recognizeing $field", symbols));

		for (std::map<trect, int>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2, char_at ++) {
			const trect& rect = it2->first;
			progress.set_percentage(100 * recognized_chars / total_chars);

			cv::Rect clip_rect(rect.x, rect.y, rect.w, rect.h);
			cv::Mat tmp;
			cv::threshold(gray(clip_rect), tmp, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

			erase_isolated_pixel(tmp, 0, 255);

			cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
			// it is white backgraound/black foreground.
			cv::morphologyEx(tmp, tmp, cv::MORPH_OPEN, element);
/*
			if (char_at == 6) {
				cv::Mat tmp2;
				cv::cvtColor(tmp, tmp2, cv::COLOR_GRAY2BGR);
				imwrite(tmp2, "3.png");
			}
*/
			cv::Mat dest;
			cv::resize(tmp, dest, cvSize(tflite_params_.width, tflite_params_.height));
			
			if (tflite_params_.color == tflite::color_gray) {
				
			} else if (tflite_params_.color == tflite::color_bgr) {
				cv::cvtColor(dest, dest, cv::COLOR_GRAY2BGR);
			} else if (tflite_params_.color == tflite::color_rgb) {
				cv::cvtColor(dest, dest, cv::COLOR_GRAY2RGB);
			} else {
				VALIDATE(false, "unknown color format");
			}

			uint32_t used_ticks_one = 0;
			wchar_t wch = tflite::inference_char(current_session_, pb_path_, sha256_, dest, &used_ticks_one);
			result.append(UCS2_to_UTF8(wch));
			used_ticks += used_ticks_one;

			recognized_chars ++;
			// SDL_Delay(100);
		}
		recognize_result_.insert(std::make_pair(line.field, tocr_result(result, used_ticks)));
	}

	return true;
}

bool ocr_controller::field_is_bound(const std::string& field) const
{
	const int size = units_.size();
	for (int at = 0; at < size; at ++) {
		ocr_unit* u = units_.find_unit(at);
		if (u->line().field == field) {
			return true;
		}
	}
	return false;
}

#include "gui/dialogs/ocr.hpp"
#include "hotkeys.hpp"

namespace tflite {

std::map<std::string, tocr_result> ocr(const config& app_cfg, CVideo& video, const surface& surf, const std::vector<std::string>& fields, const std::string& tflite, const std::string& sha256, const tflite::tparams& tflite_params, const bool bg_color_partition)
{
	VALIDATE(gui2::connectd_window().empty(), null_str);

	std::vector<std::unique_ptr<tocr_line> > lines;
	surface target;
	{
		gui2::tocr dlg(surf, lines, bg_color_partition);
		dlg.show();
		if (dlg.get_retval() != gui2::twindow::OK) {
			return std::map<std::string, tocr_result>();
		}

		tpoint offset(0, 0);
		target = dlg.target(offset);
		for (std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin(); it != lines.end(); ) {
			tocr_line& line = *it->get();
			std::map<trect, int> chars = line.chars;
			line.chars.clear();
			for (std::map<trect, int>::const_iterator it2 = chars.begin(); it2 != chars.end(); ++ it2) {
				if (it2->second == nposm) {
					continue;
				}
				trect rect = it2->first;
				rect.x += offset.x;
				rect.y += offset.y;
				line.chars.insert(std::make_pair(rect, it2->second));
			}
			if (!line.chars.empty()) {
				line.calculate_bounding_rect();
				++ it;

			} else {
				it = lines.erase(it);
			}
		}

	}

	{
		int ii = 0;
		// imwrite(target, "3.png");
	}

	hotkey::scope_changer changer(app_cfg, "hotkey_ocr");

	ocr_controller controller(app_cfg, video, target, lines, fields, tflite, sha256, tflite_params);
	controller.initialize(display::ZOOM_72);
	int ret = controller.main_loop();
	if (ret != gui2::twindow::OK) {
		return std::map<std::string, tocr_result>();
	}
	return controller.recognize_result();
}

}