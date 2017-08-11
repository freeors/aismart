#define GETTEXT_DOMAIN "rose-lib"

#include "gui/dialogs/ocr.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"
#include "font.hpp"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include <boost/bind.hpp>
#include <numeric>

#include "video.hpp"

#define bboxes_rect_2_uint64(rect)		posix_mku64(posix_mku32(rect.h, rect.w), posix_mku32(rect.y, rect.x))
#define bboxes_cvrect_2_uint64(rect)	posix_mku64(posix_mku32(rect.height, rect.width), posix_mku32(rect.y, rect.x))

namespace gui2 {

REGISTER_DIALOG(rose, ocr)

tocr::tocr(const surface& target, std::vector<std::unique_ptr<tocr_line> >& lines, const bool bg_color_partition)
	: target_(target)
	, lines_(lines)
	, bg_color_partition_(bg_color_partition)
	, verbose_(false)
	, paper_(nullptr)
	, save_(nullptr)
	, current_surf_(std::make_pair(false, surface()))
	, cv_tex_pixel_dirty_(false)
	, blended_tex_pixels_(nullptr)
	, setting_(false)
	, recognition_thread_running_(false)
	, original_local_offset_(0, 0)
	, current_local_offset_(0, 0)
	, current_render_size_(nposm, nposm)
	, thumbnail_size_(nposm, nposm)
	, next_recognize_frame_(0)
{
	VALIDATE(lines.empty(), "lines must be empty");

	// of couse, you can enable verbose when camera, but it will be heavy to save png to disk.
	VALIDATE(!verbose_ || target_.get(), null_str);
}

tocr::~tocr()
{
	VALIDATE(!avcapture_.get(), null_str);
}

surface tocr::target(tpoint& offset) const
{
	VALIDATE(!lines_.empty(), null_str);
	surface target = target_;
	if (!target_.get()) {
		VALIDATE(camera_surf_.get(), null_str);
		target = camera_surf_;
	}
	int new_width = ceil(1.0 * target->w / 72) * 72;
	int new_height = ceil(1.0 * target->h / 72) * 72;
	surface ret = create_neutral_surface(new_width, new_height);

	offset.x = (new_width - target->w) / 2;
	offset.y = (new_height - target->h) / 2;

	fill_surface(ret, 0xff808080);
	SDL_Rect dst_rect = ::create_rect(offset.x, offset.y, target->w, target->h);
	sdl_blit(target, nullptr, ret, &dst_rect);
	return ret;
}

void tocr::pre_show()
{
	window_->set_label("misc/white-background.png");

	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	paper_->set_did_draw(boost::bind(&tocr::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_mouse_leave(boost::bind(&tocr::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->set_did_mouse_motion(boost::bind(&tocr::did_mouse_motion_paper, this, _1, _2, _3));

	save_ = find_widget<tbutton>(window_, "save", false, true);
	connect_signal_mouse_left_click(
			  *save_
			, boost::bind(
				&tocr::save
				, this, boost::ref(*window_)));
	save_->set_active(false);

	if (!target_.get()) {
		start_avcapture();
	}
}

void tocr::post_show()
{
	if (!target_.get()) {
		stop_avcapture();
	}
}

const trect* point_in_conside_histogram(const std::set<trect>& conside_region, const SDL_Rect& next_char_allow_rect)
{
	for (std::set<trect>::const_iterator it = conside_region.begin(); it != conside_region.end(); ++ it) {
		const trect& rect = *it;

		if (rect.x == next_char_allow_rect.x && rect.x + rect.w <= next_char_allow_rect.x + next_char_allow_rect.w && 
			rect.y >= next_char_allow_rect.y && rect.y + rect.h <= next_char_allow_rect.y + next_char_allow_rect.h) {
			return &rect;
		}
	}
	return nullptr;
}

bool rect_overlap_rect_set(const std::set<trect>& rect_set, const trect& rect)
{
	for (std::set<trect>::const_iterator it = rect_set.begin(); it != rect_set.end(); ++ it) {
		if (it->rect_overlap(rect)) {
			return true;
		}
	}
	return false;
}

bool tbboxes::is_character(const SDL_Rect& char_rect) const
{	
	const int inner_bonus = 6;
	const int outer_bonus = 4;

	int min_x = char_rect.x - outer_bonus;
	int max_x = char_rect.x + inner_bonus;
	int min_y = char_rect.y - outer_bonus;
	int max_y = char_rect.y + inner_bonus;

	int min_x2 = char_rect.x + char_rect.w - 1 - inner_bonus;
	int max_x2 = char_rect.x + char_rect.w - 1 + outer_bonus;

	int min_y2 = char_rect.y + char_rect.h - 1 - inner_bonus;
	int max_y2 = char_rect.y + char_rect.h - 1 + outer_bonus;

	SDL_Rect over_rect {INT_MAX, INT_MAX, -1, -1};

	int low_at = 0;
	if (min_x > 0) {
		SDL_Rect low{min_x, 0, 0, 0};
		low_at = inserting_at(bboxes_rect_2_uint64(low));
	}

	SDL_Rect high{max_x2 + 1, 0, 0, 0};
	int high_at = inserting_at(bboxes_rect_2_uint64(high));

	for (int at = low_at; at < high_at; at ++) {
		const SDL_Rect rect = to_rect(at);

		int x2 = rect.x + rect.w - 1;
		int y2 = rect.y + rect.h - 1;
		if (rect.x < min_x || rect.y < min_y || x2 > max_x2 || y2 > max_y2) {
			continue;
		}

		if (rect.x < over_rect.x) {
			over_rect.x = rect.x;
		}
		if (rect.y < over_rect.y) {
			over_rect.y = rect.y;
		}
		if (x2 > over_rect.w) {
			over_rect.w = x2;
		}
		if (y2 > over_rect.h) {
			over_rect.h = y2;
		}

		if (over_rect.x >= min_x && over_rect.x <= max_x && over_rect.y >= min_y && over_rect.y <= max_y) {
			if (over_rect.w >= min_x2 && over_rect.w <= max_x2 && over_rect.h >= min_y2 && over_rect.h <= max_y2) {
				return true;
			}
		}
	}

	return false;
}

#define SPACE_LEVEL		256

void generate_mark_line_png(const std::vector<std::unique_ptr<tocr_line> >& lines, surface& src_surf, int epoch, const std::string& file)
{
	surface surf = !file.empty()? clone_surface(src_surf): src_surf;

	tsurface_2_mat_lock lock(surf);

	// mark line color
	std::vector<cv::Scalar> line_colors;
	line_colors.push_back(cv::Scalar(0, 0, 255, 255));
	line_colors.push_back(cv::Scalar(0, 255, 0, 255));
	line_colors.push_back(cv::Scalar(255, 0, 0, 255));

	int at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it, at ++) {
		const tocr_line& line = *it->get();
		cv::Scalar color = line_colors[epoch % line_colors.size()];
		for (std::map<trect, int>::const_iterator it2 = line.chars.begin(); it2 != line.chars.end(); ++ it2) {
			const trect& rect = it2->first;

			if (it2->second != SPACE_LEVEL) {
				surface text_surf = font::get_rendered_text(str_cast(it2->second), 0, 5 * twidget::hdpi_scale, font::BAD_COLOR);
				SDL_Rect dst_rect{rect.x, rect.y, text_surf->w, text_surf->h};
				sdl_blit(text_surf, nullptr, surf, &dst_rect);
			}
			cv::rectangle(lock.mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), color, it2->second != SPACE_LEVEL? 1: cv::FILLED);
		}
		if (!SDL_RectEmpty(&line.bounding_rect)) {
			cv::rectangle(lock.mat, cv::Rect(line.bounding_rect.x, line.bounding_rect.y, line.bounding_rect.w, line.bounding_rect.h), cv::Scalar(255, 255, 255, 255));

			// surface text_surf = font::get_rendered_text(str_cast(at), 0, font::SIZE_DEFAULT, font::BIGMAP_COLOR);
			surface text_surf = font::get_rendered_text(str_cast(at), 0, font::SIZE_DEFAULT, font::BAD_COLOR);
			SDL_Rect dst_rect{line.bounding_rect.x, line.bounding_rect.y, text_surf->w, text_surf->h};
			sdl_blit(text_surf, nullptr, surf, &dst_rect);
		}
	}

	if (!file.empty()) {
		imwrite(surf, file);
	}
}

SDL_Rect calculate_min_max_foreground_pixel(const cv::Mat& src, uint16_t* gray_per_row_ptr, uint16_t* gray_per_col_ptr, int min_foreground_pixels)
{
	SDL_Rect ret{INT_MAX, INT_MAX, -1, -1};
	VALIDATE(src.channels() == 1, null_str);
	bool row_valided = false;
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		for (int col = 0; col < src.cols; col ++) {
			int gray_value = data[col];
			if (gray_value == 0) {
				if (col < ret.x) {
					ret.x = col;
				}
				if (col > ret.w) {
					ret.w = col;
				}
				if (row < ret.y) {
					ret.y = row;
				}
				if (row > ret.h) {
					ret.h = row;
				}
				gray_per_row_ptr[row] ++;
				gray_per_col_ptr[col] ++;
			}
		}
		if (!row_valided && ret.x != INT_MAX) {
			if (gray_per_row_ptr[row] >= min_foreground_pixels) {
				row_valided = true;
			} else {
				ret = ::create_rect(INT_MAX, INT_MAX, -1, -1);
			}
		}
	}
	if (row_valided) {
		for (int row = ret.h; row >= ret.y; row --) {
			if (gray_per_row_ptr[row] >= min_foreground_pixels) {
				break;
			}
			ret.h = row - 1;
		}
		VALIDATE(ret.y <= ret.h, null_str);

	} else {
		VALIDATE(ret.x == INT_MAX && ret.y == INT_MAX && ret.w == -1 && ret.h == -1, null_str);
	}

	return ret;
}

SDL_Rect enclosing_rect_from_kernel_rect(const cv::Mat& src, const SDL_Rect& kernel_rect, uint16_t* gray_per_row_ptr, uint16_t* gray_per_col_ptr, int min_foreground_pixels)
{
	VALIDATE(src.channels() == 1, null_str);

	SDL_Rect base_rect {kernel_rect.x, kernel_rect.y, kernel_rect.x + kernel_rect.w - 1, kernel_rect.y + kernel_rect.h - 1};

	while (base_rect.x > 0 || base_rect.w + 1 != src.cols) {
		int min_x = base_rect.x, max_x = base_rect.w;
		// middle segment.
		for (int row = base_rect.y; row <= base_rect.h; row ++) {
			const uint8_t* data = src.ptr<uint8_t>(row);
			for (int col = base_rect.x - 1; col >= 0; col --) {
				int gray_value = data[col];
				if (gray_value != 0) {
					break;
				}
				if (col < min_x) {
					min_x = col;
				}
			}
			for (int col = base_rect.w + 1; col < src.cols; col ++) {
				int gray_value = data[col];
				if (gray_value != 0) {
					break;
				}
				if (col > max_x) {
					max_x = col;
				}
			}
		}
		VALIDATE(min_x >= 0 && max_x < src.cols, null_str);

		if (min_x != base_rect.x || max_x != base_rect.w) {
			VALIDATE(min_x <= base_rect.x && max_x >= base_rect.w, null_str);
			base_rect.x = min_x;
			base_rect.w = max_x;
			continue;
		}

		// top segment
		if (base_rect.y > 0) {
			min_x = INT_MAX;
			max_x = -1;

			const uint8_t* data = src.ptr<uint8_t>(base_rect.y - 1);
			for (int col = base_rect.x - 1; col >= 0; col --) {
				int gray_value = data[col];
				if (gray_value != 0) {
					break;
				}
				if (col < min_x) {
					min_x = col;
				}
			}
			if (min_x != INT_MAX) {
				max_x = base_rect.w;
			}
			for (int col = min_x == INT_MAX? base_rect.x: base_rect.w + 1; col < src.cols; col ++) {
				int gray_value = data[col];
				if (gray_value != 0) {
					if (col <= base_rect.w) {
						continue;
					} else {
						break;
					}
				}
				if (col < base_rect.w) {
					col = base_rect.w;
				}
				if (col > max_x) {
					max_x = col;
				}
			}
			if (max_x != -1 && min_x == INT_MAX) {
				min_x = base_rect.x;
			}

			if (min_x != INT_MAX) {
				base_rect.y --;

				VALIDATE(min_x >= 0 && max_x >= min_x && max_x < src.cols, null_str);
				if (min_x != base_rect.x || max_x != base_rect.w) {
					VALIDATE(min_x <= base_rect.x && max_x >= base_rect.w, null_str);
					base_rect.x = min_x;
					base_rect.w = max_x;
				}
				continue;
			}
		}

		// bottom segment
		if (base_rect.h + 1 != src.rows) {
			min_x = INT_MAX;
			max_x = -1;

			const uint8_t* data = src.ptr<uint8_t>(base_rect.h + 1);
			for (int col = base_rect.x - 1; col >= 0; col --) {
				int gray_value = data[col];
				if (gray_value != 0) {
					break;
				}
				if (col < min_x) {
					min_x = col;
				}
			}
			if (min_x != INT_MAX) {
				max_x = base_rect.w;
			}
			for (int col = min_x == INT_MAX? base_rect.x: base_rect.w + 1; col < src.cols; col ++) {
				int gray_value = data[col];
				if (gray_value != 0) {
					if (col <= base_rect.w) {
						continue;
					} else {
						break;
					}
				}
				if (col < base_rect.w) {
					col = base_rect.w;
				}
				if (col > max_x) {
					max_x = col;
				}
			}
			if (max_x != -1 && min_x == INT_MAX) {
				min_x = base_rect.x;
			}

			if (min_x != INT_MAX) {
				base_rect.h ++;

				VALIDATE(min_x >= 0 && max_x >= min_x && max_x < src.cols, null_str);
				if (min_x != base_rect.x || max_x != base_rect.w) {
					VALIDATE(min_x <= base_rect.x && max_x >= base_rect.w, null_str);
					base_rect.x = min_x;
					base_rect.w = max_x;
				}
				continue;
			}
		}
		break;
	}

	SDL_Rect ret = base_rect;

	//
	// second calculate
	//
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		for (int col = 0; col < src.cols; col ++) {
			int gray_value = data[col];
			if (gray_value == 0) {
				if (col >= base_rect.x && col <= base_rect.w) {
					gray_per_row_ptr[row] ++;
				}
				gray_per_col_ptr[col] ++;
			}
		}
	}

	// y-axis
	if (base_rect.y > 0) {
		// y: <---base_rect.y
		for (ret.y = base_rect.y - 1; ret.y >= 0; ret.y --) {
			if (gray_per_row_ptr[ret.y] < min_foreground_pixels) {
				break;
			}
		}
		ret.y += 1;

		for (int row = 0; row < ret.y; row ++) {
			const uint8_t* data = src.ptr<uint8_t>(row);
			for (int col = 0; col < src.cols; col ++) {
				int gray_value = data[col];
				if (gray_value == 0) {
					VALIDATE(gray_per_col_ptr[col] >=1, null_str);
					gray_per_col_ptr[col] --;
				}
			}
		}
	}
	if (base_rect.h + 1 != src.rows) {
		// h: base_rect.y + base_rect.h --->
		for (ret.h = base_rect.h + 1; ret.h < src.rows; ret.h ++) {
			if (gray_per_row_ptr[ret.h] < min_foreground_pixels) {
				break;
			}
		}
		ret.h -= 1;

		for (int row = ret.h + 1; row < src.rows; row ++) {
			const uint8_t* data = src.ptr<uint8_t>(row);
			for (int col = 0; col < src.cols; col ++) {
				int gray_value = data[col];
				if (gray_value == 0) {
					VALIDATE(gray_per_col_ptr[col] >=1, null_str);
					gray_per_col_ptr[col] --;
				}
			}
		}
	}

	// x-axis
	if (base_rect.x > 0) {
		// x: <---base_rect.x 
		for (ret.x = base_rect.x - 1; ret.x >= 0; ret.x --) {
			if (gray_per_col_ptr[ret.x] < min_foreground_pixels) {
				break;
			}
		}
		ret.x += 1;
	}
	if (base_rect.w + 1 != src.cols) {
		// w: base_rect.x + base_rect.w --->
		for (ret.w = base_rect.w + 1; ret.w < src.cols; ret.w ++) {
			if (gray_per_col_ptr[ret.w] < min_foreground_pixels) {
				break;
			}
		}
		ret.w -= 1;
	}

	ret.w = ret.w - ret.x + 1;
	ret.h = ret.h - ret.y + 1;

	return ret;
}

bool is_vertical_line(const cv::Mat& src, const cv::Mat& char_mat)
{
	const int max_line_width = 6;
	if (char_mat.cols > 6) {
		return false;
	}
	if (char_mat.rows != src.rows) {
		return false;
	}

	VALIDATE(src.channels() == 1 && char_mat.channels() == 1, null_str);
	int fg_count = 0;
	for (int row = 0; row < char_mat.rows; row ++) {
		const uint8_t* data = char_mat.ptr<uint8_t>(row);
		for (int col = 0; col < char_mat.cols; col ++) {
			int gray_value = data[col];
			if (gray_value == 0) {
				fg_count ++;
			}
		}
	}
	return 100 * fg_count / (char_mat.rows * char_mat.cols) >= 50;
}

std::string join_file_png(int round, int epoch, const std::string& short_name)
{
	std::stringstream ss;
	ss << round << "-" << epoch << "-" << short_name;
	return ss.str();
}

static const int min_threshold_width = 4;
static const int min_threshold_height = 4;
static const int min_accumulate_width = 6;

void calculate_skip_rect(const int src_cols, const uint16_t* gray_per_col_ptr, const SDL_Rect& threshold_rect, bool right, SDL_Rect& result_rect)
{
	VALIDATE(result_rect.x == 0 && result_rect.y == 0 && result_rect.w == src_cols && result_rect.h > 0, null_str);

	if (right) {
		if (!threshold_rect.x) {
			result_rect.x = 0;
			result_rect.w = threshold_rect.w;
			// right-extend w until col that has pixels
			for (int col = result_rect.w; col < src_cols; col ++) {
				if (gray_per_col_ptr[col] > 0) {
					break;
				}
				result_rect.w ++;
			}
		} else {
			result_rect.x = 0;
			if (threshold_rect.x + threshold_rect.w < src_cols) {
				// make sure that threshold_rect.w is isolated, skip should include it.
				result_rect.w = threshold_rect.x + threshold_rect.w;
				// right-extend w until col that has pixels
				for (int col = result_rect.w; col < src_cols; col ++) {
					if (gray_per_col_ptr[col] > 0) {
						break;
					}
					result_rect.w ++;
				}
			} else {
				// threshold_rect.w maybe combine with right part, skip should not include it.
				VALIDATE(threshold_rect.x + threshold_rect.w == src_cols, null_str);
				result_rect.w = threshold_rect.x;
			}
		}
		VALIDATE(result_rect.x == 0 && result_rect.w > 0 && result_rect.x + result_rect.w <= src_cols, null_str);
	} else {
		if (threshold_rect.x + threshold_rect.w == src_cols) {
			result_rect.x = threshold_rect.x;
			result_rect.w = threshold_rect.w;
			// left-extend w until col that has pixels
			for (int col = result_rect.x - 1; col >= 0; col --) {
				if (gray_per_col_ptr[col] > 0) {
					break;
				}
				result_rect.x --;
				result_rect.w ++;
			}
		} else {
			VALIDATE(threshold_rect.x + threshold_rect.w < src_cols, null_str);
			result_rect.x = threshold_rect.x;
			if (threshold_rect.x) {
				// make sure that threshold_rect.w is isolated, skip should include it.
				result_rect.w = src_cols - threshold_rect.x;
				// left-extend w until col that has pixels
				for (int col = result_rect.x - 1; col >= 0; col --) {
					if (gray_per_col_ptr[col] > 0) {
						break;
					}
					result_rect.x --;
					result_rect.w ++;
				}
			} else {
				// threshold_rect.w maybe combine with right part, skip should not include it.
				result_rect.x = threshold_rect.w;
				result_rect.w = src_cols - threshold_rect.w;
			}
		}
		VALIDATE(result_rect.x >= 0 && result_rect.w > 0 && result_rect.x + result_rect.w == src_cols, null_str);
	}
}

// @src[IN]: pixel maybe changed during function. 
enum {LOCATE_EMPTY, LOCATE_VERTICAL_LINE, LOCATE_CHAR};
int locate_character(const cv::Mat& src, const SDL_Rect& base_rect, bool right, SDL_Rect& result_rect, int& threshold_level, bool verbose, int step)
{
	if (!SDL_RectEmpty(&base_rect)) {
		// from base_rect will ignore min_estimate_width, right.
		VALIDATE(base_rect.x >= 0 && base_rect.y >= 0 && 
			base_rect.x + base_rect.w <= src.cols && base_rect.y + base_rect.h <= src.rows, null_str);
		VALIDATE(base_rect.w >= min_threshold_width && base_rect.h >= min_threshold_height, null_str);
		VALIDATE(threshold_level >= 0 && threshold_level <= 255, null_str);
	}
	result_rect.x = result_rect.y = 0;
	result_rect.w = src.cols;
	result_rect.h = src.rows;

	const int min_pixels_per_cols = 1;
	const int min_pixels_per_rows = 1;
	VALIDATE(min_pixels_per_cols > 0 && min_pixels_per_rows > 0, null_str);

	cv::Mat first_cut_mat;
	if (SDL_RectEmpty(&base_rect) || threshold_level == 0 || threshold_level == 255) {
		threshold_level = (int)cv::threshold(src, first_cut_mat, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

	} else {
		threshold_level = (int)cv::threshold(src, first_cut_mat, threshold_level, 255, CV_THRESH_BINARY);
	}

	if (verbose) {
		cv::Mat clip_origin, clip_threshold;

		cv::cvtColor(src, clip_origin, cv::COLOR_GRAY2BGR);
		cv::cvtColor(first_cut_mat, clip_threshold, cv::COLOR_GRAY2BGR);

		imwrite(surface(clip_origin), join_file_png(0, step, "0-clip-origin.png"));
		imwrite(surface(clip_threshold), join_file_png(0, step, "1-clip-threshold.png"));
	}

	erase_isolated_pixel(first_cut_mat, 0, 255);

	if (verbose) {
		cv::Mat out;		
		cv::cvtColor(first_cut_mat, out, cv::COLOR_GRAY2BGR);

		imwrite(surface(out), join_file_png(0, step, "2-clip-isolated.png"));
	}

	std::unique_ptr<uint16_t> gray_per_col(new uint16_t[src.cols]);
	uint16_t* gray_per_col_ptr = gray_per_col.get();
	memset(gray_per_col_ptr, 0, src.cols * sizeof(uint16_t));

	std::unique_ptr<uint16_t> gray_per_row(new uint16_t[src.rows]);
	uint16_t* gray_per_row_ptr = gray_per_row.get();
	memset(gray_per_row_ptr, 0, src.rows * sizeof(uint16_t));

	SDL_Rect threshold_rect = empty_rect;
	if (SDL_RectEmpty(&base_rect)) {
		threshold_rect = calculate_min_max_foreground_pixel(first_cut_mat, gray_per_row_ptr, gray_per_col_ptr, 1);
		if (threshold_rect.x != INT_MAX) {
			if (threshold_rect.w - threshold_rect.x < 2) {
				threshold_rect.w = threshold_rect.w - threshold_rect.x + 1;
				calculate_skip_rect(src.cols, gray_per_col_ptr, threshold_rect, right, result_rect);
				return LOCATE_EMPTY;
			}
			if (right) {
				// adjust x.
				// 1) first col maybe noise.
				int tmp = -1;
				for (int col = threshold_rect.x + 1; col < threshold_rect.w; col ++) {
					if (gray_per_col_ptr[col] >= min_pixels_per_cols) {
						tmp = col;
						break;
					}
				}
				if (tmp != -1) {
					threshold_rect.x = tmp;
					if (gray_per_col_ptr[tmp - 1] >= min_pixels_per_cols) {
						// maybe close together in english word, don't use > 0.
						threshold_rect.x --;
					}
					VALIDATE(threshold_rect.x < threshold_rect.w, null_str);

					// adjust w.
					tmp = -1;
					for (int col = threshold_rect.x + 1; col < threshold_rect.w; col ++) {
						if (gray_per_col_ptr[col] < min_pixels_per_cols) {
							break;
						}
						tmp = col;
					}
					if (tmp != -1) {
						threshold_rect.w = tmp;
						if (gray_per_col_ptr[tmp + 1] >= min_pixels_per_cols) {
							// maybe close together in english word, don't use > 0.
							threshold_rect.w ++;
						}
					} else {
						if (threshold_rect.x > 0) {
							// skip width=threshold_rect.x
							threshold_rect.w = threshold_rect.x;

						} else {
							threshold_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
						}
					}

				} else {
					threshold_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
				}

			} else {
				// adjust w.
				// 1) last col maybe noise.
				int tmp = -1;
				for (int col = threshold_rect.w - 1; col > threshold_rect.x; col --) {
					if (gray_per_col_ptr[col] >= min_pixels_per_cols) {
						tmp = col;
						break;
					}
				}
				if (tmp != -1) {
					threshold_rect.w = tmp;
					if (gray_per_col_ptr[tmp + 1] >= min_pixels_per_cols) {
						// maybe close together in english word, don't use > 0.
						threshold_rect.w ++;
					}
					VALIDATE(threshold_rect.x < threshold_rect.w, null_str);

					// adjust x.
					tmp = -1;
					for (int col = threshold_rect.w - 1; col > threshold_rect.x; col --) {
						if (gray_per_col_ptr[col] < min_pixels_per_cols) {
							break;
						}
						tmp = col;
					}
					if (tmp != -1) {
						threshold_rect.x = tmp;
						if (gray_per_col_ptr[tmp - 1] >= min_pixels_per_cols) {
							// maybe close together in english word, don't use > 0.
							threshold_rect.x --;
						}
					} else {
						if (threshold_rect.w + 1 < src.cols) {
							// skip width = cols - threshold_rect.w
							threshold_rect.x = threshold_rect.w;

						} else {
							threshold_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
						}
					}

				} else {
					threshold_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
				}
			}

		}
		if (threshold_rect.x == INT_MAX) {
			// all pixel in (0, 0, first_cut_mat.cols, first_cut_mat.rows) is background.
			result_rect.x = 0;
			result_rect.y = 0;
			result_rect.w = first_cut_mat.cols;
			result_rect.h = first_cut_mat.rows;
			return LOCATE_EMPTY;

		} else {
			VALIDATE(threshold_rect.w >= threshold_rect.x, null_str);
			threshold_rect.w = threshold_rect.w - threshold_rect.x + 1;
		}

	} else {
		threshold_rect = enclosing_rect_from_kernel_rect(first_cut_mat, base_rect, gray_per_row_ptr, gray_per_col_ptr, min_pixels_per_cols);
		VALIDATE(threshold_rect.x <= base_rect.x && threshold_rect.y <= base_rect.y && 
			threshold_rect.w >= base_rect.w && threshold_rect.h >= base_rect.h, null_str);
	}

	VALIDATE(threshold_rect.w <= first_cut_mat.cols, null_str);
	if (threshold_rect.w != first_cut_mat.cols) {
		VALIDATE(src.cols >= threshold_rect.x + threshold_rect.w, null_str);

		cv::Rect clip(threshold_rect.x, 0, threshold_rect.w, first_cut_mat.rows);
		VALIDATE(clip.width <= src.cols, null_str);
		first_cut_mat = first_cut_mat(clip);
	}

	if (first_cut_mat.cols < min_threshold_width) {
		VALIDATE(first_cut_mat.cols > 0, null_str);
		VALIDATE(SDL_RectEmpty(&base_rect), null_str);
		// on android, when tmp.cols == 1, morphologyEx will result to:
		// heap corruption detected by dlmalloc_read
		calculate_skip_rect(src.cols, gray_per_col_ptr, threshold_rect, right, result_rect);
		return LOCATE_EMPTY;
	}

	if (verbose) {
		cv::Mat out;
		cv::cvtColor(first_cut_mat, out, cv::COLOR_GRAY2BGR);
		imwrite(surface(out), join_file_png(0, step, "3-clip-first_cut.png"));
	}

	cv::Mat clip_hsv_closed;
	cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
	// it is white backgraound/black foreground.
	cv::morphologyEx(first_cut_mat, clip_hsv_closed, cv::MORPH_OPEN, element);

	if (verbose) {
		cv::Mat out;
		cv::cvtColor(clip_hsv_closed, out, cv::COLOR_GRAY2BGR);
		imwrite(surface(out), join_file_png(0, step, "4-clip-closed.png"));
	}

	VALIDATE(clip_hsv_closed.rows <= src.rows && clip_hsv_closed.cols <= src.cols, null_str);
	memset(gray_per_row_ptr, 0, clip_hsv_closed.rows * sizeof(uint16_t));

	std::unique_ptr<uint16_t> gray_per_col2(new uint16_t[clip_hsv_closed.cols]);
	uint16_t* gray_per_col_ptr2 = gray_per_col2.get();
	memset(gray_per_col_ptr2, 0, clip_hsv_closed.cols * sizeof(uint16_t));

	SDL_Rect contour_rect = empty_rect;
	if (SDL_RectEmpty(&base_rect)) {
		// second calculate_min_max_foreground_pixel, get this char's min rectangle.
		contour_rect = calculate_min_max_foreground_pixel(clip_hsv_closed, gray_per_row_ptr, gray_per_col_ptr2, min_pixels_per_rows);				
		if (contour_rect.x != INT_MAX) {
			VALIDATE(contour_rect.y != INT_MAX && contour_rect.w != -1 && contour_rect.h != -1, null_str);
			// first row maybe noise.(over cv::MORPH_OPEN, 1 row extend to 2 row)
			int tmp = -1;
			for (int row = contour_rect.y + 2; row < contour_rect.h; row ++) {
				if (gray_per_row_ptr[row] >= min_pixels_per_rows) {
					tmp = row;
					break;
				}
			}

			if (tmp != -1) {
				contour_rect.y = tmp;
				if (gray_per_row_ptr[tmp - 1] > 0) {
					contour_rect.y --;
					if (gray_per_row_ptr[contour_rect.y] >= min_pixels_per_rows && gray_per_row_ptr[contour_rect.y] > 0) {
						contour_rect.y --;
					}
				}
				VALIDATE(contour_rect.y < contour_rect.h, null_str);

				// last row maybe noise(over cv::MORPH_OPEN, 1 row extend to 2 row)
				tmp = -1;
				for (int row = contour_rect.h - 2; row > contour_rect.y; row --) {
					if (gray_per_row_ptr[row] >= min_pixels_per_rows) {
						tmp = row;
						break;
					}
				}
				if (tmp != -1) {
					contour_rect.h = tmp;
					if (gray_per_row_ptr[tmp + 1] > 0) {
						contour_rect.h ++;
						if (gray_per_row_ptr[contour_rect.h] >= min_pixels_per_rows && gray_per_row_ptr[contour_rect.h] > 0) {
							contour_rect.h ++;
						}
					}

				} else {
					contour_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
				}

			} else {
				contour_rect = ::create_rect(INT_MAX, INT_MAX, -1, -1);
			}
		}

		if (contour_rect.x == INT_MAX) {
			// all pixel in threshold_rect is background.
			calculate_skip_rect(src.cols, gray_per_col_ptr, threshold_rect, right, result_rect);
			return LOCATE_EMPTY;

		} else {
			VALIDATE(contour_rect.w >= contour_rect.x && contour_rect.h >= contour_rect.y, null_str);
			contour_rect.w = contour_rect.w - contour_rect.x + 1;
			contour_rect.h = contour_rect.h - contour_rect.y + 1;
		}

	} else {
		// I thank MORPH_OPEN only extend w. in order to speed, use threshold_rect.x/w.
		// SDL_Rect base_rect2 {base_rect.x - threshold_rect.x, base_rect.y, base_rect.w, base_rect.h};
		SDL_Rect base_rect2 {0, base_rect.y, threshold_rect.w, base_rect.h};
		contour_rect = enclosing_rect_from_kernel_rect(clip_hsv_closed, base_rect2, gray_per_row_ptr, gray_per_col_ptr2, min_pixels_per_rows);

		VALIDATE(contour_rect.x == 0 && contour_rect.y <= base_rect2.y && 
			contour_rect.w == base_rect2.w && contour_rect.h >= base_rect2.h, null_str);
		VALIDATE(contour_rect.x + contour_rect.w == clip_hsv_closed.cols && contour_rect.y + contour_rect.h <= clip_hsv_closed.rows, null_str);
	}

	if (contour_rect.h < min_threshold_height) {
		VALIDATE(SDL_RectEmpty(&base_rect), null_str);
		// on android, when tmp.cols == 1, morphologyEx will result to:
		// heap corruption detected by dlmalloc_read
		// all pixel in contour_rect is background.
		contour_rect.x += threshold_rect.x;
		// first cut don't cut y, do needn't adjust contour_rect.y.
		calculate_skip_rect(src.cols, gray_per_col_ptr, contour_rect, right, result_rect);
		return LOCATE_EMPTY;
	}

	result_rect.x = threshold_rect.x + contour_rect.x;
	result_rect.y = contour_rect.y;
	result_rect.w = contour_rect.w;
	result_rect.h = contour_rect.h;

	if (verbose) {
		cv::Mat colsed_mat = clip_hsv_closed(cv::Rect(contour_rect.x, contour_rect.y, contour_rect.w, contour_rect.h));
		cv::Mat out;
		cv::cvtColor(colsed_mat, out, cv::COLOR_GRAY2BGR);
		imwrite(surface(out), join_file_png(0, step, "5-clip-contour.png"));
	}

	if (!SDL_RectEmpty(&base_rect)) {
		// is vertical line.
		bool vertical_line = is_vertical_line(src, first_cut_mat(cv::Rect(contour_rect.x, contour_rect.y, contour_rect.w, contour_rect.h)));
		if (vertical_line) {
			return LOCATE_VERTICAL_LINE;
		}
	}
	return LOCATE_CHAR;
}

void average_bg_color(const cv::Mat& src, int& r, int& g, int& b)
{
	VALIDATE(src.rows > 0 && src.cols > 0 && src.channels() == 4, null_str);

	r = g = b = 0;

	const int channels = src.channels();
	const int ncols = src.cols * channels;
	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		for (int col = 0; col < ncols; col += channels) {
			b += data[col];
			g += data[col + 1];
			r += data[col + 2];
		}
	}

	int count = src.rows * src.cols;
	r /= count;
	g /= count;
	b /= count;
}

static void generate_threshold_demo_png()
{
	surface surf = image::get_image(game_config::preferences_dir + "/3.png");

	cv::Mat gray;
	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, gray, cv::COLOR_BGRA2GRAY);
	}

	std::vector<std::pair<cv::Rect, int> > clips;
	clips.push_back(std::make_pair(cv::Rect(245, 555, 26, 21), nposm));
	clips.push_back(std::make_pair(cv::Rect(240, 548, 31, 35), nposm));
	clips.push_back(std::make_pair(cv::Rect(240, 548, 31, 35), 0));

	SDL_Rect bounding_rect{0, 0, 0, 0};
	const int horizontal_gap = 2, vertical_gap = 4, threshold_text_width = 36;
	int max_width = 0;
	for (std::vector<std::pair<cv::Rect, int> >::const_iterator it = clips.begin(); it != clips.end(); ++ it) {
		const cv::Rect& clip = it->first;
		if (clip.width > max_width) {
			max_width = clip.width;
		}
		bounding_rect.h += vertical_gap + clip.height;
	}
	bounding_rect.h += vertical_gap;
	bounding_rect.w = horizontal_gap + max_width + horizontal_gap + max_width + threshold_text_width;

	surface result = create_neutral_surface(bounding_rect.w, bounding_rect.h);
	fill_surface(result, 0xffa6a9e5);

	int x = 0, y = 0;
	cv::Mat out;
	for (std::vector<std::pair<cv::Rect, int> >::iterator it = clips.begin(); it != clips.end(); ++ it) {
		const cv::Rect& clip = it->first;
		x = horizontal_gap;
		y += vertical_gap;

		int threshold = -1;
		const bool otsu = it->second == nposm;
		if (it->second == nposm) {
			threshold = cv::threshold(gray(clip), out, 0, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);
			it->second = threshold;
		} else {
			int distance = std::distance(clips.begin(), it);
			VALIDATE(it->second < distance, null_str);

			threshold = cv::threshold(gray(clip), out, clips[it->second].second, 255, CV_THRESH_BINARY);
		}

		surface thresholded_surf = create_neutral_surface(clip.width, clip.height);
		tsurface_2_mat_lock lock2(thresholded_surf);
		cv::cvtColor(out, lock2.mat, cv::COLOR_GRAY2BGRA);

		SDL_Rect src_rect{clip.x, clip.y, clip.width, clip.height};
		SDL_Rect dst_rect{x, y, clip.width, clip.height};
		sdl_blit(surf, &src_rect, result, &dst_rect);

		dst_rect.x += max_width + horizontal_gap;
		sdl_blit(thresholded_surf, nullptr, result, &dst_rect);

		SDL_Color color = otsu? font::NORMAL_COLOR: font::BAD_COLOR;
		surface text_surf = font::get_rendered_text(str_cast(threshold), 0, 18, color);
		dst_rect.x += max_width;
		dst_rect.y += (dst_rect.h - text_surf->h) / 2;
		sdl_blit(text_surf, nullptr, result, &dst_rect);

		y += clip.height;
	}

	imwrite(result, "4.png");
}

#define calculate_detect_width(average_width)		(average_width * 4 / 3) // in order to split with next/before, don't tow lardge.
#define calculate_max_blank_width(average_width)	((average_width) * 3 / 2)
#define SPACE_BONUS_WIDTH	2
#define calculate_min_space_width(average_width)	((average_width) * 2 / 3 + SPACE_BONUS_WIDTH)
const int min_bbox_width = 4;
const int min_bbox_height = 4;

// false: has finish grow.
// true: encounter exception, include blank, vertical line, other color background. 
//       caller require continue grow from new row.
void grow_line(const tbboxes& bboxes3, const cv::Mat& src, bool verbose, const cv::Mat& gray, 
	const std::vector<std::unique_ptr<tocr_line> >& lines, const int line_at, const int epoch, const SDL_Rect& this_first_char, 
	SDL_Rect& accumulate_rect, int& valid_chars, const bool bg_color_partition, const bool right)
{
	tocr_line& line = *lines[line_at].get();

	if (right) {
		VALIDATE(line.chars.empty() && valid_chars == 1, null_str);
	}

	bool calibrating = line.chars.empty()? true: false;

	// this line can be in below tow pixel.
	int maybe_same_line_left_x = -1, maybe_same_line_right_x = INT_MAX;

	for (int at = 0; at < line_at; at ++) {
		const SDL_Rect& bounding_rect = lines[at].get()->bounding_rect;
		if (bounding_rect.y < this_first_char.y + this_first_char.h && this_first_char.y < bounding_rect.y + bounding_rect.h) {
			if (this_first_char.x < bounding_rect.x) {
				if (bounding_rect.x < maybe_same_line_right_x) {
					maybe_same_line_right_x = bounding_rect.x - 1;
				}
			} else if (this_first_char.x >= bounding_rect.x + bounding_rect.w) {
				// I hope below VALIDATE is true always, but grow maybe extend vertical, result VALIDATE false when left-grow.
				// VALIDATE(this_first_char.x >= bounding_rect.x + bounding_rect.w, null_str);
				maybe_same_line_left_x = bounding_rect.x + bounding_rect.w;
			}
		}
	}

	const int min_threshold_gap = 1;
	// calculate next_cut_x.
	int next_cut_x = right? gray.cols: -1;
	if (right) {
		if (maybe_same_line_right_x != INT_MAX) {
			next_cut_x = maybe_same_line_right_x - min_threshold_gap;
		}

	} else {
		if (maybe_same_line_left_x != -1) {
			next_cut_x = maybe_same_line_left_x + min_threshold_gap - 1;
		}
	}

	int const_detect_width = calculate_detect_width(accumulate_rect.w / valid_chars); // it will effect cv::threshold(...)
	int max_blank_width = calculate_max_blank_width(accumulate_rect.w / valid_chars);
	int min_space_width = calculate_min_space_width(accumulate_rect.w / valid_chars);
	const int vbonus_size = 2;
	int blank_start = (right || calibrating)? -1: this_first_char.x - 1;
	int space_start = (right || calibrating)? -1: this_first_char.x - 1;

	int detect_width = const_detect_width;
	SDL_Rect desire_clip {-1, -1, 0, 0}, base_rect {-1, -1, 0, 0};

	// first pixel of new character will start from maybe_current_x.
	int maybe_current_x = this_first_char.x;
	if (right) {
		maybe_current_x = this_first_char.x;
		if (maybe_current_x + min_threshold_width > next_cut_x) {
			return;
		}

	} else {
		maybe_current_x = this_first_char.x - min_threshold_gap - detect_width;
		if (maybe_current_x <= next_cut_x) {
			// will use next_cut_x + 1 replace maybe_current_x, 'next_cut_x + 1 - maybe_current_x' is the difference.
			detect_width -= next_cut_x + 1 - maybe_current_x;
			if (detect_width < min_threshold_width) {
				return;
			}
			maybe_current_x = next_cut_x + 1;
		}
	}

	int inserted_at = 0;

	int current_r = -1, current_g = -1, current_b = -1;
	int threshold_level = nposm;
	SDL_Rect last_clip_rect {-1, -1, 0, 0};
	while (true) {
		if (right) {
			if (maybe_current_x >= next_cut_x) {
				// has reach to next line.
				break;
			}

		} else {
			if (maybe_current_x <= next_cut_x) {
				// has reach to before line or 0.
				break;
			}
		}
		VALIDATE(maybe_current_x >= 0, null_str);

		// grow to left/right
		SDL_Rect clip_rect {maybe_current_x, accumulate_rect.y / valid_chars - vbonus_size, detect_width, accumulate_rect.h / valid_chars + vbonus_size * 2};
		detect_width = const_detect_width; // reset detect_width

		if (desire_clip.x != -1) {
			clip_rect = desire_clip;
		}
		if (right) {
			if (clip_rect.x + clip_rect.w > next_cut_x) {
				clip_rect.w = next_cut_x - clip_rect.x;
				VALIDATE(clip_rect.w > 0, null_str);
			}
		} else {
			if (clip_rect.x <= next_cut_x) {
				clip_rect.w -= next_cut_x + 1 - clip_rect.x;
				VALIDATE(clip_rect.w > 0, null_str);
				clip_rect.x = next_cut_x + 1;
			}
		}

		if (clip_rect.y < 0) {
			clip_rect.h -= 0 - clip_rect.y;
			VALIDATE(clip_rect.h > 0, null_str);
			clip_rect.y = 0;
		}
		if (clip_rect.y + clip_rect.h > gray.rows) {
			clip_rect.h = gray.rows - clip_rect.y;
			VALIDATE(clip_rect.h > 0, null_str);
		}

		VALIDATE(clip_rect.w > 0 && clip_rect.h > 0, null_str);
		if (calibrating) {
			if (clip_rect == last_clip_rect) {
				// avoid dead lock.
				break;
			}
		}
		last_clip_rect = clip_rect;

		SDL_Rect char_rect;
		int ret = locate_character(gray(cv::Rect(clip_rect.x, clip_rect.y, clip_rect.w, clip_rect.h)), desire_clip.x == -1? empty_rect: base_rect, right, char_rect, threshold_level, verbose && epoch == 0 && right && line_at == 1 && inserted_at == 11, 5);
		VALIDATE(char_rect.w > 0, null_str);

		if (epoch == 0 && right && line_at == 1 && inserted_at == 11) {
			int ii = 0;
		}
		if (ret == LOCATE_CHAR) {
			SDL_Rect result_rect {clip_rect.x + char_rect.x, clip_rect.y + char_rect.y, char_rect.w, char_rect.h};

			VALIDATE(result_rect.x + result_rect.w <= clip_rect.x + clip_rect.w, null_str);
			VALIDATE(result_rect.y + result_rect.h <= clip_rect.y + clip_rect.h, null_str);

			if (calibrating) {
				desire_clip = ::create_rect(-1, -1, 0, 0);
			}

			if (desire_clip.x == -1) {
				VALIDATE(desire_clip.y == -1 && desire_clip.w == 0 && desire_clip.h == 0, null_str);

				bool second_locate = false;
				if (result_rect.y == clip_rect.y || (result_rect.y + result_rect.h == clip_rect.y + clip_rect.h)) {
					// vertical result to second locate. calculate y/h is after judge horizontal.
					second_locate = true;
				}

				if (right) {
					const int x_end = result_rect.x + result_rect.w;
					if (second_locate || (x_end == clip_rect.x + clip_rect.w && x_end != next_cut_x)) {
						// if x_end has reach to end, disable extend.
						desire_clip.x = clip_rect.x;
						desire_clip.w = clip_rect.w + const_detect_width;

						second_locate = true;
					}

				} else {
					if (second_locate || (result_rect.x == clip_rect.x && result_rect.x - 1 != next_cut_x)) {
						// x_end has reach to end, disable extend.
						desire_clip.x = clip_rect.x - const_detect_width;
						desire_clip.w = clip_rect.w + const_detect_width;

						if (desire_clip.x <= next_cut_x) {
							desire_clip.w -= next_cut_x + 1 - desire_clip.x;
							VALIDATE(desire_clip.w > 0, null_str);
							desire_clip.x = next_cut_x + 1;
						}
						second_locate = true;
					}
				}

				if (second_locate) {
					desire_clip.y = result_rect.y - clip_rect.h / 3;
					desire_clip.h = clip_rect.h + clip_rect.h / 3;

					if (desire_clip.y < 0) {
						desire_clip.h -= -1 * desire_clip.y;
						desire_clip.y = 0;
					}
					desire_clip.h += clip_rect.h / 3;
				}

				if (desire_clip.x != -1) {
					if (right) {
						VALIDATE(desire_clip.x == result_rect.x || desire_clip.x == clip_rect.x, null_str);
					} else {
						VALIDATE(desire_clip.x <= result_rect.x, null_str);
					}
					VALIDATE(desire_clip.y <= result_rect.y && desire_clip.w >= result_rect.w && desire_clip.h >= result_rect.h, null_str);
					if (right) {
						base_rect = ::create_rect(result_rect.x - desire_clip.x, result_rect.y - desire_clip.y, result_rect.w, result_rect.h);
					} else {
						base_rect = ::create_rect(result_rect.x - desire_clip.x, result_rect.y - desire_clip.y, result_rect.w, result_rect.h);
					}
					maybe_current_x = desire_clip.x;
					continue;
				}
			} else {
				desire_clip = ::create_rect(-1, -1, 0, 0);
			}

			for (std::map<trect, int>::const_iterator it = line.chars.begin(); it != line.chars.end(); ++ it) {
				VALIDATE(!it->first.rect_overlap2(result_rect), null_str);
			}


			if (bg_color_partition && !line.chars.empty()) {
				const trect& rect = right? line.chars.rbegin()->first: line.chars.begin()->first;
				int overlap_x = 0, overlap_w = 0;
				if (right) {
					overlap_x = rect.x + rect.w + 1;
					overlap_w = result_rect.x - rect.x - rect.w - 2;
				} else {
					overlap_x = result_rect.x + result_rect.w + 1;
					overlap_w = rect.x - result_rect.x - result_rect.w - 2;
				}
				if (overlap_w > 0 && (rect.y < result_rect.y + result_rect.h && result_rect.y < rect.y + rect.h)) {
					int res_y = std::max<int>(rect.y, result_rect.y);
					int res_h = std::min<int>(rect.y + rect.h, result_rect.y + result_rect.h) - res_y;

					cv::Rect overlap{overlap_x, res_y, overlap_w, res_h};
					int r, g, b;
					average_bg_color(src(overlap), r, g, b);

					if (current_r != -1) {
						VALIDATE(current_g != -1 && current_b != -1, null_str);
						const int bg_color_threshold = 20;
						int r_diff = abs(r - current_r);
						int g_diff = abs(g - current_g);
						int b_diff = abs(b - current_b);
						if (r_diff > bg_color_threshold || g_diff > bg_color_threshold || b_diff > bg_color_threshold) {
							break;
						}
					}
					current_r = r;
					current_g = g;
					current_b = b;
				}
			}

			bool halt = false;
			bool space = false;
			if (!calibrating) {
				const bool valid_char = bboxes3.is_character(result_rect);

				// judge blank
				VALIDATE(blank_start > 0 && space_start > 0,  null_str);
				if (right) {
					VALIDATE(result_rect.x >= blank_start, null_str);
					if (!valid_char && result_rect.x + result_rect.w - blank_start >= max_blank_width) {
						if (!line.chars.empty()) {
							// if this line has char, don't append this rect.
							break;
						}
						// this line hasn't char, in order to caputre postion, insert a char. but less this char's width.
						halt = true;
						VALIDATE(max_blank_width >= min_space_width, null_str);
						result_rect.x = blank_start;
						result_rect.w = min_space_width;

					} else {
						VALIDATE(result_rect.x >= space_start, null_str);
						if (result_rect.x - space_start >= min_space_width) {
							space = true;
							result_rect.w = result_rect.x - space_start - SPACE_BONUS_WIDTH;
							result_rect.x = space_start;

						}
					}

				} else {
					VALIDATE(result_rect.x < blank_start, null_str);
					if (!valid_char && blank_start - result_rect.x >= max_blank_width) {
						if (!line.chars.empty()) {
							// if this line has char, don't append this rect.
							break;
						}
						// this line hasn't char, in order to caputre postion, insert a char. but less this char's width.
						halt = true;
						VALIDATE(max_blank_width >= min_space_width, null_str);
						result_rect.w = min_space_width;
						result_rect.x = blank_start - result_rect.w;
					}

					VALIDATE(result_rect.x < space_start, null_str);
					if (space_start - result_rect.x - result_rect.w >= min_space_width) {
						space = true;
						const int tmp_w = result_rect.w;
						result_rect.w = space_start - result_rect.x - result_rect.w - SPACE_BONUS_WIDTH;
						result_rect.x = result_rect.x + tmp_w + SPACE_BONUS_WIDTH;

					}
				}

				if (space) {
					VALIDATE(result_rect.w >= min_space_width - SPACE_BONUS_WIDTH && result_rect.h > 0, null_str);
					for (std::map<trect, int>::const_iterator it = line.chars.begin(); it != line.chars.end(); ++ it) {
						VALIDATE(!it->first.rect_overlap(result_rect), null_str);
					}
					threshold_level = SPACE_LEVEL;

				} else if (valid_char) {
					if (!line.chars.empty()) {
						accumulate_rect.y += result_rect.y;
						accumulate_rect.h += result_rect.h;
						accumulate_rect.w += posix_max(result_rect.w, min_accumulate_width);
						valid_chars ++;

					} else {
						VALIDATE(valid_chars == 1, null_str);
						accumulate_rect.y = result_rect.y;
						accumulate_rect.h = result_rect.h;
						accumulate_rect.w = posix_max(result_rect.w, min_accumulate_width);
					}
				} else {
					threshold_level = nposm;

				}
				line.chars.insert(std::make_pair(result_rect, threshold_level));
				inserted_at ++;
				if (halt) {
					break;
				}

			} else {
				accumulate_rect.y = result_rect.y;
				accumulate_rect.h = result_rect.h;
				accumulate_rect.w = posix_max(result_rect.w, min_accumulate_width);

				calibrating = false;
			}
			if (!space) {
				const_detect_width = calculate_detect_width(accumulate_rect.w / valid_chars);
				detect_width = const_detect_width; // next loop should use this detect_width.
				max_blank_width = calculate_max_blank_width(accumulate_rect.w / valid_chars);
				min_space_width = calculate_min_space_width(accumulate_rect.w / valid_chars);
			}

			if (right) {
				if (!space) {
					maybe_current_x = result_rect.x + result_rect.w + min_threshold_gap;
					if (threshold_level != nposm) {
						blank_start = result_rect.x + result_rect.w;
					}
					space_start = result_rect.x + result_rect.w;
				} else {
					maybe_current_x = result_rect.x + result_rect.w;
					space_start = maybe_current_x;
				}
			} else {
				if (!space) {
					maybe_current_x = result_rect.x - min_threshold_gap - const_detect_width;
					if (threshold_level != nposm) {
						blank_start = result_rect.x - 1;
					}
					space_start = result_rect.x - 1;

				} else {
					maybe_current_x = result_rect.x - const_detect_width;
					space_start = result_rect.x - 1;
				}
			}
			
		} else if (ret == LOCATE_EMPTY) {
			if (desire_clip.x != -1) {
				desire_clip = ::create_rect(-1, -1, 0, 0);
			}
			// no character. but char_rect indicate require skip's rect.
			if (right) {
				VALIDATE(char_rect.x == 0 && char_rect.w > 0, null_str);
				maybe_current_x = clip_rect.x + char_rect.w;
			} else {
				VALIDATE(char_rect.x + char_rect.w == clip_rect.w && char_rect.w > 0, null_str);
				maybe_current_x = clip_rect.x - char_rect.w;
			}

		} else {
			VALIDATE(ret == LOCATE_VERTICAL_LINE, null_str);
			break;
		}
	}
}

bool is_overlap_before_lines(const trect& rect, const std::vector<std::unique_ptr<tocr_line> >& lines, int cut_at)
{
	VALIDATE(cut_at >= 0 && cut_at < (int)lines.size(), null_str);
	int line_at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it, line_at ++) {
		if (line_at >= cut_at) {
			return false;
		}
		const SDL_Rect& bounding_rect = it->get()->bounding_rect;
		if (rect.rect_overlap(bounding_rect)) {
			return true;
		}
	}
	return false;
}

bool did_compare_row(const std::unique_ptr<tocr_line>& a, const std::unique_ptr<tocr_line>& b)
{
	VALIDATE(!a->chars.empty() && !b->chars.empty(), null_str);

	if (a->chars.size() != b->chars.size()) {
		return a->chars.size() > b->chars.size();
	}
	const trect& a_char = a->chars.begin()->first;
	const trect& b_char = b->chars.begin()->first;
	if (a_char.y != b_char.y) {
		return b_char.y > a_char.y;
	}

	VALIDATE(a_char.x != b_char.x, null_str);
	return b_char.x > a_char.x;
}

void grow_lines(const tbboxes& bboxes3, const cv::Mat& src, bool verbose_, const cv::Mat& gray, std::vector<std::unique_ptr<tocr_line> >& lines, int fixed_rows, const bool bg_color_partition, const int epoch)
{
	std::vector<std::unique_ptr<tocr_line> >::iterator lines_it = lines.begin();
	// sort lines.
	std::advance(lines_it, fixed_rows);
	std::stable_sort(lines_it, lines.end(), did_compare_row);

	// grow line.
	int line_at = fixed_rows;
	for (int line_at = fixed_rows; line_at < (int)lines.size(); ) {
		tocr_line& line = *lines[line_at].get();
		SDL_Rect accumulate_rect {0, INT_MAX, 0, -1};

		for (std::map<trect, int>::iterator it2 = line.chars.begin(); it2 != line.chars.end();) {
			const trect& rect = it2->first;
			if (is_overlap_before_lines(rect, lines, line_at)) {
				line.chars.erase(it2 ++);
				continue;
			}

			if (rect.y < accumulate_rect.y) {
				accumulate_rect.y = rect.y;
			}
			accumulate_rect.w += rect.w;
			if (rect.y + rect.h > accumulate_rect.h) {
				accumulate_rect.h = rect.y + rect.h;
			}
			++ it2;
		}
		if (line.chars.empty()) {
			std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin();
			std::advance(it, line_at);
			lines.erase(it);
			continue;
		}

		SDL_Rect average_rect {0, accumulate_rect.y, 
			accumulate_rect.w / (int)line.chars.size(), accumulate_rect.h - accumulate_rect.y};

		SDL_Rect this_first_char = line.chars.begin()->first.to_SDL_Rect();
		int valid_chars = 1;

		std::map<trect, int> vchars = line.chars;
		line.chars.clear();

		grow_line(bboxes3, src, verbose_, gray, lines, line_at, epoch, this_first_char, average_rect, valid_chars, bg_color_partition, true);
		if (!line.chars.empty()) {
			this_first_char = line.chars.begin()->first.to_SDL_Rect(); // right grow has update first character.
		}

		grow_line(bboxes3, src, verbose_, gray, lines, line_at, epoch, this_first_char, average_rect, valid_chars, bg_color_partition, false);

		if (!line.chars.empty()) {
			line.calculate_bounding_rect();
			line_at ++;

			// rest chars
			const size_t vchars_size = vchars.size();
			for (std::map<trect, int>::iterator it2 = vchars.begin(); it2 != vchars.end();) {
				const trect& rect = it2->first;
				if (rect.rect_overlap2(line.bounding_rect)) {
					vchars.erase(it2 ++);
					continue;
				}
				++ it2;
			}
			if (vchars_size != vchars.size() && !vchars.empty()) {
				// whe right, no valid char, when left, has a black, it will result to vchars_size == vchars.size().
				// it will make lines.size() more and more, goto dead lock at last.
				lines.push_back(std::unique_ptr<tocr_line>(new tocr_line(vchars)));
			}

		} else {
			std::vector<std::unique_ptr<tocr_line> >::iterator it = lines.begin();
			std::advance(it, line_at);
			lines.erase(it);
		}
	}
}

void erase_bboxes_base_lines(tbboxes& bboxes3, const std::vector<std::unique_ptr<tocr_line> >& lines)
{
	const int count = lines.size();
	std::unique_ptr<SDL_Rect> bounding_rects(new SDL_Rect[count]);
	SDL_Rect* bounding_rects_ptr = bounding_rects.get();
	int at = 0;
	for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it, at ++) {
		const SDL_Rect& bounding_rect = it->get()->bounding_rect;
		memcpy(bounding_rects_ptr + at, &bounding_rect, sizeof(SDL_Rect));
	}

	bool erase;
	for (int at = 0; at < bboxes3.cache_vsize;) {
		const SDL_Rect rect = bboxes3.to_rect(at);
		erase = false;
		for (int at2 = 0; at2 < count; at2 ++) {
			if (rects_overlap(rect, bounding_rects_ptr[at2])) {
				erase = true;
				break;
			}
		}
		if (erase) {
			bboxes3.erase(rect);
		} else {
			at ++;
		}
	}
}

void tbboxes::resize_cache(int size)
{
	size = posix_align_ceil(size, 1024);
	VALIDATE(size >= 0, null_str);

	if (size > cache_size_) {
		uint64_t* tmp = (uint64_t*)malloc(size * sizeof(uint64_t));
		memset(tmp, 0, size * sizeof(uint64_t));
		if (cache) {
			if (cache_vsize) {
				memcpy(tmp, cache, cache_vsize * sizeof(uint64_t));
			}
			free(cache);
		}
		cache = tmp;
		cache_size_ = size;
	}
}

int tbboxes::find(const uint64_t key) const
{
	int start = 0;
	int end = cache_vsize - 1;
	int mid = (end - start) / 2 + start;
	uint64_t cache_key;
	while (mid - start > 1) {
		cache_key = cache[mid];
		if (key < cache_key) {
			end = mid;

		} else if (key > cache_key) {
			start = mid;

		} else {
			return mid;
		}
		mid = (end - start) / 2 + start;
	}

	int at = start;
	for (; at <= end; at ++) {
		cache_key = cache[at];
		if (cache_key == key) {
			return at;
		}
	}

	VALIDATE(false, null_str);
	return nposm;
}

// caller should insert key in 'at' that it return.
int tbboxes::inserting_at(const uint64_t key) const
{
	int start = 0;
	int end = cache_vsize - 1;
	int mid = (end - start) / 2 + start;
	uint64_t cache_key;
	while (mid - start > 1) {
		cache_key = cache[mid];
		if (key < cache_key) {
			end = mid;

		} else if (key > cache_key) {
			start = mid;

		} else {
			return mid;
		}
		mid = (end - start) / 2 + start;
	}

	int at = start;
	for (; at <= end; at ++) {
		cache_key = cache[at];
		if (cache_key >= key) {
			return at;
		}
	}

	return at;
}

SDL_Rect tbboxes::to_rect(int at) const
{
	VALIDATE(at >= 0 && at < cache_vsize, null_str);
	uint64_t key = cache[at];
	uint32_t xy = posix_hi32(key);
	uint32_t wh = posix_lo32(key);

	return SDL_Rect{posix_hi16(xy), posix_lo16(xy), posix_hi16(wh), posix_lo16(wh)};
}

void tbboxes::insert(const cv::Rect& rect)
{
	VALIDATE(rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0, null_str);

	resize_cache(cache_vsize + 1);
	uint64_t key = bboxes_cvrect_2_uint64(rect);
	int at = inserting_at(key);
	if (at < cache_vsize && cache[at] == key) {
		return;
	}

	if (cache_vsize - at) {
		memmove(&cache[at + 1], &cache[at], (cache_vsize - at) * sizeof(uint64_t));
	}

	cache[at] = key;
	cache_vsize ++;
}

void tbboxes::erase(const SDL_Rect& rect)
{
	uint64_t key = bboxes_rect_2_uint64(rect);
	int at = find(key);

	cache[at] = 0;
	if (at < cache_vsize - 1) {
		memcpy(&(cache[at]), &(cache[at + 1]), (cache_vsize - at - 1) * sizeof(uint64_t));

	}
	cache[cache_vsize - 1] = 0;

	cache_vsize --;
}

void tocr::locate_lines(surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines)
{
	lines.clear();

	tsurface_2_mat_lock lock(surf);
	cv::Mat gray;
	cv::cvtColor(lock.mat, gray, cv::COLOR_BGRA2GRAY);
	const int image_area = gray.rows * gray.cols;
	// const int delta = 1; // easypr using
	const int delta = 5; // 1823, 1044
	const int min_area = 30;
	const double max_area_ratio = 0.05;

	std::vector<std::vector<cv::Point> > inv_msers, msers;
    std::vector<cv::Rect> inv_bboxes, bboxes;

	cv::Ptr<cv::MSER> mser = cv::MSER::create(delta, min_area, int(max_area_ratio * image_area), 0.25, 0.2);
	mser->detectRegions(gray, msers, bboxes);

/*
	cv::Ptr<cv::MSER2> mser = cv::MSER2::create(delta, min_area, int(max_area_ratio * image_area));
	mser->detectRegions(gray, inv_msers, inv_bboxes, msers, bboxes);
*/
	tbboxes bboxes3;
	for (std::vector<cv::Rect>::const_iterator it = bboxes.begin(); it != bboxes.end(); ++ it) {
		const cv::Rect& rect = *it;
		if (rect.width < min_bbox_width) {
			continue;
		}
		if (rect.height < min_bbox_height) {
			continue;
		}

		bboxes3.insert(rect);
	}


	if (verbose_) {
		// mser and conside png
		cv::Mat mser_mat = lock.mat.clone();
		for (std::vector<cv::Rect>::const_iterator it = bboxes.begin(); it != bboxes.end(); ++ it) {
			const cv::Rect& rect = *it;
			cv::rectangle(mser_mat, rect, cv::Scalar(119, 119, 119, 255));
		}
		imwrite(surface(mser_mat), join_file_png(0, 0, "mser.png"));
	}

	const int max_epochs = 2;
	for (int epoch = 0; epoch < max_epochs; epoch ++) {
		locate_lines_epoch(surf, lock.mat, gray, bboxes3, lines, epoch);
	}
}

void tocr::locate_lines_epoch(surface& surf, const cv::Mat& src, const cv::Mat& gray, tbboxes& bboxes3, std::vector<std::unique_ptr<tocr_line> >& lines, const int epoch)
{
	std::map<tpoint, int> regions;

	std::map<tpoint, int>::iterator find;
	int max_value = 0;
	tpoint max_point(0, 0);
	for (int at = 0; at < bboxes3.cache_vsize; at ++) {
		const SDL_Rect rect = bboxes3.to_rect(at);
		find = regions.find(tpoint(rect.w, rect.h));
		if (find != regions.end()) {
			find->second ++;
			if (find->second > max_value) {
				max_value = find->second;
				max_point = tpoint(rect.w, rect.h);
			}
		} else {
			regions.insert(std::make_pair(tpoint(rect.w, rect.h), 1));
		}
	}

	const int min_conside_height = 16;
	VALIDATE(min_conside_height >= min_bbox_height, null_str);

	std::set<trect> conside_region;
	const int conside_min_value = (int)(0.618 * max_value);
	for (int at = 0; at < bboxes3.cache_vsize; at ++) {
		const SDL_Rect rect = bboxes3.to_rect(at);
		if (rect.h < min_conside_height) {
			// continue;
		}

		find = regions.find(tpoint(rect.w, rect.h));
		VALIDATE(find != regions.end(), null_str);
		if (find != regions.end()) {
			if (find->second >= conside_min_value) {
				VALIDATE(!conside_region.count(trect(rect.x, rect.y, rect.w, rect.h)), null_str);
				conside_region.insert(rect);
			}
		}
	}

	if (verbose_) {
		// mser and conside png
		cv::Mat conside_mat = src.clone();
		for (int at = 0; at < bboxes3.cache_vsize; at ++) {
			const SDL_Rect rect = bboxes3.to_rect(at);
			if (conside_region.count(rect)) {
				continue;
			}
			cv::Scalar color(119, 119, 119, 255);
			cv::rectangle(conside_mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), color);
		}
		for (std::set<trect>::const_iterator it = conside_region.begin(); it != conside_region.end(); ++ it) {
			const trect& rect = *it;
			cv::Scalar color(0, 0, 255, 255);
			cv::rectangle(conside_mat, cv::Rect(rect.x, rect.y, rect.w, rect.h), color);
		}
		imwrite(surface(conside_mat), join_file_png(epoch, 1, "conside.png"));

		// histogam png.
		int hscale = 16, vscale = 24;
		cv::Mat histogram_mat = cv::Mat::zeros(max_value * vscale, regions.size() * hscale, CV_8UC3);
		int at = 0;
		for (std::map<tpoint, int>::const_iterator it = regions.begin(); it != regions.end(); ++ it, at ++) {
			int height = it->second * vscale;
			cv::Scalar color(255, 255, 255);
			if (at) {
				if (at % 100 == 0) {
					color = cv::Scalar(255, 0, 0);
				} else if (at % 50 == 0) {
					color = cv::Scalar(0, 255, 0);
				} else if (at % 10 == 0) {
					color = cv::Scalar(0, 0, 255);
				} 
			}
			cv::rectangle(histogram_mat, cv::Rect(at * hscale, max_value * vscale - height, hscale, height), color);
		}
		cv::line(histogram_mat, cv::Point(0, (max_value - conside_min_value) * vscale), cv::Point(histogram_mat.cols - 1, (max_value - conside_min_value) * vscale), cv::Scalar(0, 0, 255));
		imwrite(surface(histogram_mat), join_file_png(epoch, 2, "histogram.png"));
	}

	const int fixed_rows = (int)lines.size();
	std::set<trect> conside_rect;
	for (std::set<trect>::const_iterator it = conside_region.begin(); it != conside_region.end(); ++ it) {
		const trect& rect = *it;
		if (rect_overlap_rect_set(conside_rect, rect)) {
			continue;
		}
		int x = rect.x + rect.w;
		SDL_Rect accumulate_rect {rect.x, rect.y, rect.w, rect.h};
		SDL_Rect average_rect = accumulate_rect;

		std::set<trect> valid_chars;
		valid_chars.insert(rect);
		const int can_line_valid_chars = 3;
		const int min_char_gap = 2;
		cv::Rect new_rect;
		for (int bonus = min_char_gap; bonus < average_rect.w * 2; bonus ++) {
			const int x2 = x + bonus;
			const SDL_Rect next_char_allow_rect{x2, average_rect.y - average_rect.h / 4, average_rect.w * 3 / 2, average_rect.h * 3 / 2};
			const trect* new_rect = point_in_conside_histogram(conside_region, next_char_allow_rect);
			if (new_rect && !rect_overlap_rect_set(conside_rect, *new_rect)) {
				VALIDATE(!rect_overlap_rect_set(valid_chars, *new_rect), null_str);
				valid_chars.insert(*new_rect);
				bonus = min_char_gap; // find a valid char, reset relative variable.
				x = new_rect->x + new_rect->w;
				accumulate_rect.y += new_rect->y;
				accumulate_rect.w += new_rect->w;
				accumulate_rect.h += new_rect->h;
				average_rect.y = accumulate_rect.y / valid_chars.size();
				average_rect.w = accumulate_rect.w / valid_chars.size();
				average_rect.h = accumulate_rect.h / valid_chars.size();
			}
		}
		if (valid_chars.size() >= can_line_valid_chars) {
			conside_rect.insert(valid_chars.begin(), valid_chars.end());

			std::map<trect, int> valid_chars2;
			for (std::set<trect>::const_iterator it2 = valid_chars.begin(); it2 != valid_chars.end(); ++ it2) {
				valid_chars2.insert(std::make_pair(*it2, 0));
			}
			lines.push_back(std::unique_ptr<tocr_line>(new tocr_line(valid_chars2)));
		}
	}

	if (verbose_) {
		generate_mark_line_png(lines, surf, epoch, join_file_png(epoch, 3, "line-strong-seed.png"));
	}

	grow_lines(bboxes3, src, verbose_, gray, lines, fixed_rows, bg_color_partition_, epoch);

	if (verbose_) {
		generate_mark_line_png(lines, surf, epoch, join_file_png(epoch, 5, "line-grown.png"));
	}

	erase_bboxes_base_lines(bboxes3, lines);
}

void tocr::texecutor::DoWork()
{
	ocr_.recognition_thread_running_ = true;
	while (ocr_.avcapture_.get()) {
		if (ocr_.setting_) {
			// main thread is setting.
			// seting action may be risk recognition_mutex_.
			SDL_Delay(10);
			continue;
		}
		threading::lock lock(ocr_.recognition_mutex_);
		if (!ocr_.avcapture_.get()) {
			continue;
		}
		if (!ocr_.current_surf_.first) {
			SDL_Delay(10);
			continue;
		}

		std::vector<std::unique_ptr<tocr_line> > lines;
		ocr_.locate_lines(ocr_.current_surf_.second, lines);

		{
			threading::lock lock(ocr_.variable_mutex_);
			ocr_.current_surf_.first = false;

			if (ocr_.avcapture_.get() && !lines.empty()) {
				// user maybe want last look ocr_.lines_
				surface& src_surf = ocr_.current_surf_.second;

				if (ocr_.camera_surf_.get()) {
					VALIDATE(ocr_.camera_surf_->w == src_surf->w && ocr_.camera_surf_->h == src_surf->h, null_str);
					surface_lock lock(ocr_.camera_surf_);
					Uint32* pixels = lock.pixels();
					memcpy(pixels, src_surf->pixels, src_surf->w * src_surf->h * 4); 

				} else {
					ocr_.camera_surf_ = clone_surface(src_surf);
				}

				ocr_.lines_.clear();
				for (std::vector<std::unique_ptr<tocr_line> >::const_iterator it = lines.begin(); it != lines.end(); ++ it) {
					ocr_.lines_.push_back(std::unique_ptr<tocr_line>(new tocr_line(*it->get())));
				}

				generate_mark_line_png(ocr_.lines_, src_surf, 0, null_str);

				memcpy(ocr_.blended_tex_pixels_, src_surf->pixels, src_surf->w * src_surf->h * 4); 
				ocr_.cv_tex_pixel_dirty_ = true;
			}
		}
	}
	ocr_.recognition_thread_running_ = false;
}

void tocr::start_avcapture()
{
	VALIDATE(!target_.get(), null_str);
	VALIDATE(!recognition_thread_running_ && !avcapture_.get(), null_str);

	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	avcapture_.reset(new tavcapture(*this, tpoint(1280, nposm)));
	avcapture_->set_did_draw_slice(boost::bind(&tocr::did_paper_draw_slice, this, _1, _2, _3, _4, _5, _6));
	paper_->set_timer_interval(30);

	current_surf_ = std::make_pair(false, surface());

	executor_.reset(new texecutor(*this));
}

void tocr::stop_avcapture()
{
	VALIDATE(!target_.get() && window_, null_str);

	{
		tsetting_lock setting_lock(*this);
		threading::lock lock(recognition_mutex_);
		avcapture_.reset();
		paper_->set_timer_interval(0);

		current_surf_ = std::make_pair(false, surface());
	}
	executor_.reset();
}

void tocr::did_paper_draw_slice(bool remote, const cv::Mat& frame, const texture& tex, const SDL_Rect& draw_rect, bool new_frame, texture& cv_tex)
{
	SDL_Renderer* renderer = get_renderer();
	VALIDATE(!target_.get(), null_str);

	if (!blended_tex_pixels_) {
		int pitch = 0;
		SDL_LockTexture(cv_tex.get(), NULL, (void**)&blended_tex_pixels_, &pitch);
	}

	if (new_frame && !current_surf_.first) {
		const int max_value = posix_max(frame.cols, frame.rows);
		const int frames = avcapture_->vrenderer(false)->frames();
		if (max_value <= 640 || frames >= next_recognize_frame_) {
			threading::lock lock(variable_mutex_);
			if (!current_surf_.second.get()) {
				current_surf_.second = create_neutral_surface(frame.cols, frame.rows);
			}
			memcpy(current_surf_.second->pixels, frame.data, frame.cols * frame.rows * 4); 
			current_surf_.first = true;

			const int recognize_threshold = 5;
			next_recognize_frame_ = frames + recognize_threshold;
		}
	}

	const tpoint ratio_size = calculate_adaption_ratio_size(draw_rect.w, draw_rect.h, frame.cols, frame.rows);
	SDL_Rect dst {draw_rect.x + (draw_rect.w - ratio_size.x) / 2, draw_rect.y + (draw_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y};
	current_render_size_ = tpoint(dst.w, dst.h);

	// renderer video pixel
	threading::lock lock(variable_mutex_);
	if (camera_surf_.get()) {
		VALIDATE(!lines_.empty(), null_str);
		if (cv_tex_pixel_dirty_) {
			SDL_UnlockTexture(cv_tex.get());
			cv_tex_pixel_dirty_ = false;
		}
		
		SDL_RenderCopy(renderer, cv_tex.get(), nullptr, &dst);
		
	} else {
		SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
	}

	dst = calculate_thumbnail_rect(dst.x, dst.y, dst.w, dst.h);
	SDL_RenderCopy(renderer, tex.get(), NULL, &dst);
}

SDL_Rect tocr::calculate_thumbnail_rect(const int xsrc, const int ysrc, const int frame_width, const int frame_height)
{
	thumbnail_size_.x = frame_width / 4;
	thumbnail_size_.y = frame_height / 4;

	tpoint ratio_size = calculate_adaption_ratio_size(thumbnail_size_.x, thumbnail_size_.x, frame_width, frame_height);

	SDL_Rect ret;
	ret.w = ratio_size.x;
	ret.h = ratio_size.y;
	ret.x = xsrc + original_local_offset_.x + current_local_offset_.x;
	ret.y = ysrc + original_local_offset_.y + current_local_offset_.y;

	return ret;
}

void tocr::did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);

	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), NULL, &widget_rect);
	}

	surface surf;
	SDL_Rect dst;
	texture thumbnail_tex;
	if (target_.get()) {
		surf = clone_surface(target_);
	
		locate_lines(surf, lines_);
		generate_mark_line_png(lines_, surf, 0, null_str);

		const tpoint ratio_size = calculate_adaption_ratio_size(widget_rect.w, widget_rect.h, surf->w, surf->h);
		dst = ::create_rect(widget_rect.x + (widget_rect.w - ratio_size.x) / 2, widget_rect.y + (widget_rect.h - ratio_size.y) / 2, ratio_size.x, ratio_size.y);
		current_render_size_ = tpoint(dst.w, dst.h);

		render_surface(renderer, surf, nullptr, &dst);

		thumbnail_tex = SDL_CreateTextureFromSurface(renderer, target_);

		dst = calculate_thumbnail_rect(dst.x, dst.y, dst.w, dst.y);
		
		if (!target_.get()) {
			// when target_.get() is slow. require optimaze.
			SDL_RenderCopy(renderer, thumbnail_tex.get(), NULL, &dst);
		}

	} else {
		avcapture_->draw_slice(renderer, false, widget_rect, true);
	}

	if (!lines_.empty()) {
		save_->set_active(true);
	}
}

void tocr::did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/)
{
	original_local_offset_ += current_local_offset_;
	current_local_offset_.x = current_local_offset_.y = 0;

	int width = current_render_size_.x;
	int height = current_render_size_.y;
	if (original_local_offset_.x < 0) {
		original_local_offset_.x = 0;
	} else if (original_local_offset_.x > width - thumbnail_size_.x) {
		original_local_offset_.x = width - thumbnail_size_.x;
	}
	if (original_local_offset_.y < 0) {
		original_local_offset_.y = 0;
	} else if (original_local_offset_.y > height - thumbnail_size_.y) {
		original_local_offset_.y = height - thumbnail_size_.y;
	}

	current_render_size_.x = current_render_size_.y = nposm;
}

void tocr::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}
	current_local_offset_ = last - first;
	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

void tocr::save(twindow& window)
{
	{
		// generate_threshold_demo_png();
		// return;
	}

	window.set_retval(twindow::OK);
}

} // namespace gui2

