#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/runner.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/track.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/tflite_schema.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "rose_config.hpp"
#include "filesystem.hpp"
#include "font.hpp"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/video.hpp>

#include <boost/bind.hpp>

#include "easypr/core/plate_recognize.h"

namespace gui2 {

REGISTER_DIALOG(aismart, runner)

trunner::trunner(const tscript& script, const ttflite& tflite)
	: script_(script)
	, tflite_(tflite)
	, last_coordinate_(construct_null_coordinate())
	, recognition_thread_running_(false)
	, rng_(12345)
	, current_surf_(std::make_pair(false, surface()))
	, setting_(false)
	, next_recognize_frame_(0)
{
}

trunner::~trunner()
{
	VALIDATE(!avcapture_.get(), null_str);
}

void trunner::pre_show()
{
	window_->set_label("misc/white-background.png");

	tbutton* button = find_widget<tbutton>(window_, "cancel", false, true);
	button->set_icon("misc/back.png");

	button = find_widget<tbutton>(window_, "ok", false, true);
	button->set_visible(twidget::HIDDEN);

	pre_base(*window_);
}

void trunner::post_show()
{
	stop_avcapture();
}

void trunner::app_first_drawn()
{
	const std::string file = get_binary_file_location("tflites", tflite_.id + "/" + tflite_.id + ".tflits");
	VALIDATE(file.find(tflite_.path()) == 0, null_str);

	bool s = tflite::load_model(file, current_session_, game_config::showcase_sha256);
	VALIDATE(s, std::string("err load: ") + file);

	VALIDATE(!avcapture_.get() && blits_.empty(), null_str);
	if (script_.source == source_camera) {
		start_avcapture();
		thread_->Start();
	}

	if (script_.scenario == scenario_classifier) {
		if (script_.source == source_img) {
			classifier_image();
		} else {
			did_draw_paper(*paper_, paper_->get_draw_rect(), false);
		}

	} else if (script_.scenario == scenario_detect) {
		if (script_.source == source_img) {

		} else {
			did_draw_paper(*paper_, paper_->get_draw_rect(), false);
		}

	} else if (script_.scenario == scenario_pr) {
		if (script_.source == source_img) {
			pr_image();

		} else {
			did_draw_paper(*paper_, paper_->get_draw_rect(), false);
		}

	}
}

void trunner::pre_base(twindow& window)
{
	paper_ = find_widget<ttrack>(window_, "paper", false, true);
	paper_->set_did_draw(boost::bind(&trunner::did_draw_paper, this, _1, _2, _3));
	paper_->set_did_left_button_down(boost::bind(&trunner::did_left_button_down_paper, this, _1, _2));
	paper_->set_did_mouse_leave(boost::bind(&trunner::did_mouse_leave_paper, this, _1, _2, _3));
	paper_->set_did_mouse_motion(boost::bind(&trunner::did_mouse_motion_paper, this, _1, _2, _3));
}

void trunner::DoWork()
{
	recognition_thread_running_ = true;
	while (avcapture_.get()) {
		if (setting_) {
			// main thread is setting.
			// seting action may be risk recognition_mutex_.
			SDL_Delay(10);
			continue;
		}
		threading::lock lock(recognition_mutex_);
		if (!avcapture_.get()) {
			continue;
		}
		if (!current_surf_.first) {
			SDL_Delay(10);
			continue;
		}
		if (script_.scenario == scenario_classifier) {
			VALIDATE(current_session_.valid(), null_str);

			std::string result = classifier_surface(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else if (script_.scenario == scenario_detect) {
			VALIDATE(current_session_.valid(), null_str);

			std::string result = example_detector_internal(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else if (script_.scenario == scenario_pr) {
			VALIDATE(current_session_.valid(), null_str);

			std::string result = pr_surface(current_surf_.second);
			current_surf_.first = false;
			{
				threading::lock lock(variable_mutex_);
				result_ = result;
			}

		} else {
			SDL_Delay(10);
		}
	}
	recognition_thread_running_ = false;
}

void trunner::OnWorkStart()
{
}

void trunner::OnWorkDone()
{
}

std::string trunner::classifier_surface(const surface& surf)
{
	VALIDATE(surf, null_str);
	std::unique_ptr<tflite::Interpreter>& interpreter = current_session_.interpreter;

	std::vector<std::string> label_strings = tflite::load_labels_txt(tflite_.path() + "/" + tflite::labels_txt);
	VALIDATE(label_strings.size() > 1, null_str);

	// Read the Grace Hopper image.
	cv::Mat src;
	{
		tsurface_2_mat_lock lock(surf);
		if (script_.color == tflite::color_gray) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2GRAY);
		} else if (script_.color == tflite::color_bgr) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2BGR);
		} else if (script_.color == tflite::color_rgb) {
			cv::cvtColor(lock.mat, src, cv::COLOR_BGRA2RGB);
		} else {
			VALIDATE(false, "unknown color format");
		}
	}

	const int image_channels = src.channels();
	const int sourceRowBytes = src.cols * image_channels;

	tpoint ratio_size = calculate_adaption_ratio_size(src.cols, src.rows, script_.width, script_.height);
	int image_width = ratio_size.x;
	int image_height = ratio_size.y;

	int marginX = (src.cols - image_width) / 2;
	int marginY = (src.rows - image_height) / 2;

/*
	if (src.rows <= src.cols) {
		// height <= width
		image_width = image_height = src.rows;
		marginX = (src.cols - image_width) / 2;

	} else {
		image_width = image_height = src.cols;
		marginY = (src.rows - image_width) / 2;
	}
*/
	src = src(cv::Rect(marginX, marginY, image_width, image_height)).clone();

	{
		threading::lock variable_lock(variable_mutex_);
		rects_.clear();
		SDL_Rect rc {marginX, marginY, image_width, image_height};
		rects_.push_back(std::make_pair((float)0.0, rc));
	}

	const int wanted_input_width = script_.width;
	const int wanted_input_height = script_.height;
	const int wanted_input_channels = script_.color == tflite::color_gray? 1: 3;
	VALIDATE(wanted_input_channels == image_channels, null_str);

	int input = interpreter->inputs()[0];
	uint8_t* out = interpreter->typed_tensor<uint8_t>(input);
/*
	cv::Mat dest;
	cv::resize(src, dest, cvSize(wanted_input_width, wanted_input_height));
	for (int y = 0; y < wanted_input_height; ++y) {
		const uint8_t* in_row = dest.ptr(y);
		uint8_t* out_row = out + (y * wanted_input_width * wanted_input_channels);
		for (int x = 0; x < wanted_input_width; ++ x) {
			memcpy(out_row, in_row, wanted_input_width * wanted_input_channels);
		}
	}
*/

	for (int y = 0; y < wanted_input_height; ++y) {
		const int in_y = (y * image_height) / wanted_input_height;
		const uint8_t* in_row = src.ptr(in_y);
		uint8_t* out_row = out + (y * wanted_input_width * wanted_input_channels);
		for (int x = 0; x < wanted_input_width; ++x) {
			const int in_x = (x * image_width) / wanted_input_width;
			const uint8_t* in_pixel = in_row + (in_x * image_channels);
			uint8_t* out_pixel = out_row + (x * wanted_input_channels);
			for (int c = 0; c < wanted_input_channels; ++c) {
				out_pixel[c] = in_pixel[c];
			}
		}
	}

	std::stringstream res;
	std::vector<std::pair<float, int> > top_results;

	uint32_t used_ms = tflite::invoke_classifier(*interpreter, label_strings.size() - 1, 5, script_.kthreshold, top_results);
	if (used_ms == nposm) {
		res << "Failed to invoke!";
		return res.str();
	}

	res.precision(3);
	for (const auto& result : top_results) {
		const float confidence = result.first;
		const int index = result.second;

		res << " - " << confidence << "  ";

		// Write out the result as a string
		if (index < (int)label_strings.size()) {
			// just for safety: theoretically, the output is under 1000 unless there
			// is some numerical issues leading to a wrong prediction.
			res << label_strings[index];
		} else {
			res << "Prediction: " << index;
		}

		res << "\n";
	}

	res << " - use " << used_ms << " ms";

	return res.str();
}

void trunner::insert_blits(std::vector<image::tblit>& blits, const surface& surf, const std::string& result, std::vector<std::pair<float, SDL_Rect> >& rects)
{
	blits.clear();
	surface text_surf = font::get_rendered_text(result, paper_->get_width(), font::SIZE_DEFAULT, font::BLACK_COLOR);
	blits.push_back(image::tblit(text_surf, (paper_->get_width() - text_surf->w) / 2, 0, text_surf->w, text_surf->h));

	const int outer_h = paper_->get_height() - text_surf->h;
	tpoint size = calculate_adaption_ratio_size(paper_->get_width(), outer_h, surf->w, surf->h);
	const int xsrc = (paper_->get_width() - size.x) / 2;
	const int ysrc = text_surf->h;
	blits.push_back(image::tblit(surf, (paper_->get_width() - size.x) / 2, text_surf->h, size.x, size.y));

	char score_str[32];
	const double width_ratio = 1.0 * size.x / surf->w;
	const double height_ratio = 1.0 * size.y / surf->h;
	for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& rect = it->second;

		blits.push_back(image::tblit(image::BLITM_FRAME, xsrc + rect.x * width_ratio, ysrc + rect.y * height_ratio, rect.w * width_ratio, rect.h * height_ratio, 0xffff0000));

		if (script_.scenario == scenario_pr) {
			sprintf(score_str, "%0.2f", it->first);
			text_surf = font::get_rendered_text(score_str, 0, font::SIZE_DEFAULT, font::GOOD_COLOR);
			SDL_Rect score_dst{(int)(xsrc + rect.x * width_ratio), (int)(ysrc + rect.y * height_ratio), text_surf->w, text_surf->h};
			blits.push_back(image::tblit(text_surf, score_dst.x, score_dst.y, score_dst.w, score_dst.h));
		}
	}
}

void trunner::classifier_image()
{
	surface surf = image::get_image(script_.img);
	VALIDATE(surf, null_str);
	std::string result = classifier_surface(surf);

	insert_blits(blits_, surf, result, rects_);

	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

void trunner::pr_image()
{
	surface surf = image::get_image(script_.img);
	VALIDATE(surf, null_str);
	std::string result = pr_surface(surf);

	insert_blits(blits_, surf, result, rects_);

	did_draw_paper(*paper_, paper_->get_draw_rect(), false);
}

// Converts an encoded location to an actual box placement with the provided
// box priors.
static void DecodeLocation(const float* encoded_location, const float* box_priors, float* decoded_location) 
{
	bool non_zero = false;
	for (int i = 0; i < 4; ++i) {
		const float curr_encoding = encoded_location[i];
		non_zero = non_zero || curr_encoding != 0.0f;

		const float mean = box_priors[i * 2];
		const float std_dev = box_priors[i * 2 + 1];

		float currentLocation = curr_encoding * std_dev + mean;

		currentLocation = std::max(currentLocation, 0.0f);
		currentLocation = std::min(currentLocation, 1.0f);
		decoded_location[i] = currentLocation;
	}
}

static float DecodeScore(float encoded_score) 
{ 
	return 1 / (1 + exp(-encoded_score)); 
}

static void cv_draw_rectangle(cv::RNG& rng, const cv::Mat& img, const cv::Rect& rect)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255), 255));
}

static void cv_draw_rectangle2(const cv::Mat& img, const cv::Rect& rect, const uint32_t color)
{
	cv::rectangle(img, rect.tl(), rect.br(), cv::Scalar(color & 0xff, (color & 0xff00) >> 8, (color & 0xff0000) >> 16, (color & 0xff000000) >> 24));
}

std::string trunner::example_detector_internal(surface& surf)
{
	return "Now not support";
/*
	std::stringstream result;

	const std::string data_path = game_config::path + "/" + game_config::generate_app_dir(game_config::app) + "/tensorflow";
	const std::string pb_path = data_path + "/mobile_multibox_v1a/multibox_model.pb";

	tensorflow::Status s = tensorflow2::load_model(pb_path, current_session_);
	if (!s.ok()) {
		result << "load model fail: " << s;
		return result.str();
	}
	std::unique_ptr<tensorflow::Session>& session = current_session_.second;

	int32_t num_detections = 5;
	int32_t num_boxes = 784;
	if (locations_.empty()) {
		const std::string imagenet_comp_path = data_path + "/mobile_multibox_v1a/multibox_location_priors.txt";

		tfile file(imagenet_comp_path, GENERIC_READ, OPEN_EXISTING);
		VALIDATE(file.valid(), null_str);
		int64_t fsize = file.read_2_data();
		int start = nposm;
		const char* ptr = nullptr;
		for (int at = 0; at < fsize; at ++) {
			const char ch = file.data[at];
			if (ch == '\r' || ch == '\n') {
				if (start != nposm) {
					std::string line(file.data + start, at - start);
					std::vector<float> tokens;
					CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
					for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
						locations_.push_back(*it);
					}

					start = nposm;
				}
			} else if (start == nposm) {
				start = at;
			}
		}
		if (start != nposm) {
			std::string line(file.data + start, fsize - start);
			std::vector<float> tokens;
			CHECK(tensorflow::str_util::SplitAndParseAsFloats(line, ',', &tokens));
			for (std::vector<float>::const_iterator it = tokens.begin(); it != tokens.end(); ++ it) {
				locations_.push_back(*it);
			}
		}
	}
	CHECK_EQ(locations_.size(), num_boxes * 8);

	// Read the Grace Hopper image.
	{
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGRA2RGBA);
	}

	int image_width = surf->w;
	int image_height = surf->h;
	int image_channels = 4;
	const int wanted_width = 224;
	const int wanted_height = 224;
	const int wanted_channels = 3;
	const float input_mean = 128.0f;
	const float input_std = 128.0f;
	VALIDATE(image_channels >= wanted_channels, null_str);
	tensorflow::Tensor image_tensor(
		tensorflow::DT_FLOAT,
		tensorflow::TensorShape({
		1, wanted_height, wanted_width, wanted_channels }));
	auto image_tensor_mapped = image_tensor.tensor<float, 4>();

	surface_lock dst_lock(surf);
	tensorflow::uint8* in = (uint8_t*)(dst_lock.pixels());
	// tensorflow::uint8* in_end = (in + (image_height * image_width * image_channels));
	float* out = image_tensor_mapped.data();
	for (int y = 0; y < wanted_height; ++y) {
		const int in_y = (y * image_height) / wanted_height;
		tensorflow::uint8* in_row = in + (in_y * image_width * image_channels);
		float* out_row = out + (y * wanted_width * wanted_channels);
		for (int x = 0; x < wanted_width; ++x) {
			const int in_x = (x * image_width) / wanted_width;
			tensorflow::uint8* in_pixel = in_row + (in_x * image_channels);
			float* out_pixel = out_row + (x * wanted_channels);
			for (int c = 0; c < wanted_channels; ++c) {
				out_pixel[c] = (in_pixel[c] - input_mean) / input_std;
			}
		}
	}

	result << " - " << locations_.size() << ", " << image_width << "x" << image_height;

	uint32_t start = SDL_GetTicks();

	std::string input_layer = "ResizeBilinear";
	std::string output_score_layer = "output_scores/Reshape";
	std::string output_location_layer = "output_locations/Reshape";
	std::vector<tensorflow::Tensor> outputs;
	tensorflow::Status run_status = session->Run({ {input_layer, image_tensor} },
		{output_score_layer, output_location_layer}, {}, &outputs);

	if (!run_status.ok()) {
		result << "Running model failed: " << run_status;
		tensorflow::LogAllRegisteredKernels();
		return result.str();
	}

	uint32_t end = SDL_GetTicks();

	tensorflow::string status_string = run_status.ToString();
	result << " - " << status_string << "\n";

	tensorflow::Tensor& scores = outputs[0];
	const tensorflow::TTypes<float>::Flat scores_flat = scores.flat<float>();
	const int how_many_labels = scores_flat.size();

	const tensorflow::Tensor& encoded_locations = outputs[1];
	auto locations_encoded = encoded_locations.flat<float>();

	tsurface_2_mat_lock lock(surf);
	cv::cvtColor(lock.mat, lock.mat, cv::COLOR_RGBA2BGRA);

	threading::lock variable_lock(variable_mutex_);
	rects_.clear();
	const float kThreshold = 0.1f;
	for (int pos = 0; pos < how_many_labels; ++pos) {
		const float score = DecodeScore(scores_flat(pos));

		if (score < kThreshold) {
			continue;
		}

		float decoded_location[4];
		DecodeLocation(&locations_encoded(pos * 4), &locations_[pos * 8], decoded_location);

		float left = decoded_location[0] * image_width;
		float top = decoded_location[1] * image_height;
		float right = decoded_location[2] * image_width;
		float bottom = decoded_location[3] * image_height;

		rects_.push_back(std::make_pair(score, SDL_Rect({(int)left, (int)top, (int)(right - left), (int)(bottom - top)})));
	}

	result << " - use " << end - start << " ms";

	return result.str();
*/
}

std::string trunner::pr_surface(surface& surf)
{
	tsurface_2_mat_lock lock(surf);
	cv::Mat src = lock.mat.clone();
	cv::cvtColor(src, src, cv::COLOR_BGRA2BGR);

	uint32_t start = SDL_GetTicks();

	easypr::CPlateRecognize pr;
	pr.setLifemode(true);
	pr.setDebug(false);
	// pr.setMaxPlates(1);
	// pr.setDetectType(easypr::PR_DETECT_COLOR | easypr::PR_DETECT_SOBEL);
	pr.setDetectType(easypr::PR_DETECT_CMSER);

	//vector<string> plateVec;
	std::vector<easypr::CPlate> plateVec;

	std::stringstream result_ss;
	int result = pr.plateRecognize(src, plateVec);

	std::vector<std::pair<float, SDL_Rect> > rects;

	const std::string tflite_path = tflite_.tflits_path();

	if (result == 0) {
		int at = 0;
		for (std::vector<easypr::CPlate>::const_iterator it = plateVec.begin(); it != plateVec.end(); ++ it, at ++) {
			const easypr::CPlate& plate = *it;
			result_ss << " - " << conv_ansi_utf8_2(plate.getPlateStr(), true) << "(easypr)\n";

			const cv::Rect bounding = plate.getPlatePos().boundingRect();

			SDL_Rect rect = {bounding.x, bounding.y, bounding.width, bounding.height};
			rects.push_back(std::make_pair((float)plate.getPlateScore(), rect));
/*
			Mat plateMat = plate.getPlateMat();
			int channels = plateMat.channels();
			imwrite(plateMat, str_cast(at) + ".png");
*/
			std::string result;
			const std::vector<easypr::CCharacter> chars = plate.getCopyOfReutCharacters();
			int char_at = 0;
			for (std::vector<easypr::CCharacter>::const_iterator it = chars.begin(); it != chars.end(); ++ it, char_at ++) {
				cv::Mat char_mat = it->getCharacterMat();
				if (!char_mat.rows || !char_mat.cols) {
					continue;
				}
				VALIDATE(char_mat.channels() == 1, null_str);
				// if (plate.getPlateColor() != easypr::Color::BLUE) {
					for (int row = 0; row < char_mat.rows; row ++) {
						uint8_t* data = char_mat.ptr<uint8_t>(row);
						for (int col = 0; col < char_mat.cols; col ++) {
							*(data + col) = 255 - data[col];
						}
					}
				// }

				cv::Mat dest;
				cv::resize(char_mat, dest, cvSize(script_.width, script_.height));
			
				if (script_.color == tflite::color_gray) {
				
				} else if (script_.color == tflite::color_bgr) {
					cv::cvtColor(dest, dest, cv::COLOR_GRAY2BGR);
				} else if (script_.color == tflite::color_rgb) {
					cv::cvtColor(dest, dest, cv::COLOR_GRAY2RGB);
				} else {
					VALIDATE(false, "unknown color format");
				}

				wchar_t wch = tflite::inference_char(current_session_, tflite_path, game_config::showcase_sha256, dest, nullptr);
				result.append(UCS2_to_UTF8(wch));
			}
			result_ss << "   " << result << "\n";
		}
	}

	uint32_t end = SDL_GetTicks();

	result_ss << " - use " << end - start << " ms";

	{
		threading::lock variable_lock(variable_mutex_);
		rects_ = rects;
	}

	return result_ss.str();
}

void trunner::stop_avcapture()
{
	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	avcapture_.reset();
	paper_->set_timer_interval(0);

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	rects_.clear();
}

void trunner::start_avcapture()
{
	VALIDATE(current_session_.valid() && !avcapture_.get(), null_str);

	tsetting_lock setting_lock(*this);
	threading::lock lock(recognition_mutex_);

	avcapture_.reset(new tavcapture(*this));
	start_ticks_ = SDL_GetTicks();
	avcapture_->set_did_draw_slice(boost::bind(&trunner::did_paper_draw_slice, this, _1, _2, _3, _4, _5));
	paper_->set_timer_interval(30);

	current_surf_ = std::make_pair(false, surface());
	result_.clear();
	rects_.clear();
}

void trunner::did_paper_draw_slice(bool remote, const cv::Mat& frame, const texture& frame_tex, const SDL_Rect& draw_rect, bool new_frame)
{
	SDL_Renderer* renderer = get_renderer();
	if (script_.source == source_camera) {
		if (new_frame && !current_surf_.first) {
			const int max_value = posix_max(frame.cols, frame.rows);
			const int frames = avcapture_->vrenderer(false)->frames();
			if (max_value <= 640 || frames >= next_recognize_frame_) {
				if (!current_surf_.second.get()) {
					current_surf_.second = create_neutral_surface(frame.cols, frame.rows);
				}
				memcpy(current_surf_.second->pixels, frame.data, frame.cols * frame.rows * 4); 
				current_surf_.first = true;

				const int recognize_threshold = 5;
				next_recognize_frame_ = frames + recognize_threshold;
			}
		}

	}

	tpoint ratio_size = calculate_adaption_ratio_size(paper_->get_width(), paper_->get_height(), frame.cols, frame.rows);
	const int xsrc = paper_->get_x() + (paper_->get_width() - ratio_size.x) / 2;
	const int ysrc = paper_->get_y() + (paper_->get_height() - ratio_size.y) / 2;
	SDL_Rect dst {xsrc, ysrc, ratio_size.x, ratio_size.y};
	SDL_RenderCopy(renderer, frame_tex.get(), NULL, &dst);

	std::vector<std::pair<float, SDL_Rect> > rects;
	std::string result;
	{
		threading::lock lock(variable_mutex_);
		rects = rects_;
/*
		std::stringstream ss;
		uint32_t diff_ms = (SDL_GetTicks() - start_ticks_);
		if (diff_ms == 0) {
			diff_ms = 1;
		}
		float fps = 1.0 * avcapture_->vrenderer(false)->frames() / diff_ms;
		ss << result_ << "\n";
		ss << "fps: " << (int)(fps * 1000);
		ss << ", " << (diff_ms / 1000) << "s";

		result = ss.str();
*/
		result = result_;
	}
	surface text_surf;
	char score_str[32];
	const double width_ratio = 1.0 * dst.w / frame.cols;
	const double height_ratio = 1.0 * dst.h / frame.rows;
	for (std::vector<std::pair<float, SDL_Rect> >::const_iterator it = rects.begin(); it != rects.end(); ++ it) {
		const SDL_Rect& rect = it->second;

		render_rect_frame(renderer, SDL_Rect{(int)(dst.x + rect.x * width_ratio), int(dst.y + rect.y * height_ratio), int(rect.w * width_ratio), int(rect.h * height_ratio)}, 0xffff0000);

		if (script_.scenario == scenario_pr) {
			sprintf(score_str, "%0.2f", it->first);
			text_surf = font::get_rendered_text(score_str, 0, font::SIZE_DEFAULT, font::GOOD_COLOR);
			SDL_Rect score_dst{(int)(dst.x + rect.x * width_ratio), (int)(dst.y + rect.y * height_ratio), text_surf->w, text_surf->h};
			render_surface(renderer, text_surf, NULL, &score_dst);
		}
	}

	if (!result.empty()) {
		text_surf = font::get_rendered_text(result, 0, font::SIZE_LARGE, font::GOOD_COLOR);
		dst.w = text_surf->w;
		dst.h = text_surf->h;
		render_surface(renderer, text_surf, NULL, &dst);
	}
}

void trunner::did_left_button_down_paper(ttrack& widget, const tpoint& coordinate)
{
	last_coordinate_ = coordinate;
}

void trunner::did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}
/*	
	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last_coordinate_.x - first.x, last_coordinate_.y - first.y);
	
		tsurface_2_mat_lock lock(persist_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		did_draw_paper(*paper_, paper_->get_draw_rect(), false);
	}
*/
}

void trunner::did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last)
{
	if (is_null_coordinate(first)) {
		return;
	}

	last_coordinate_ = last;
/*
	if (current_example_ == mouse) {
		cv::Rect rect(first.x - widget.get_x(), first.y - widget.get_y(), last.x - first.x, last.y - first.y);
		fill_surface(temperate_surf_, 0);
		tsurface_2_mat_lock lock(temperate_surf_);
		cv_draw_rectangle(rng_, lock.mat, rect);
		did_draw_paper(*paper_, paper_->get_draw_rect(), false);
	}
*/
}

void trunner::did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn)
{
	SDL_Renderer* renderer = get_renderer();
	ttrack::tdraw_lock lock(renderer, widget);
	if (!bg_drawn) {
		SDL_RenderCopy(renderer, widget.background_texture().get(), nullptr, &draw_rect);
	}

	for (std::vector<image::tblit>::const_iterator it = blits_.begin(); it != blits_.end(); ++ it) {
		const image::tblit& blit = *it;
		image::render_blit(renderer, blit, widget.get_x(), widget.get_y());
	}

	if (avcapture_.get()) {
		avcapture_->draw_slice(renderer, false, draw_rect, true);
	}
}

} // namespace gui2