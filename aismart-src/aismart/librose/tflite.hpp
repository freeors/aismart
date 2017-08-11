#ifndef LIBROSE_TFLITE_HPP_INCLUDED
#define LIBROSE_TFLITE_HPP_INCLUDED

#include "ocr/ocr.hpp"
#include "filesystem.hpp"

#include "tensorflow/contrib/lite/model.h"
#include <opencv2/core/mat.hpp>

struct surface;
class CVideo;

namespace tflite {

extern const std::string labels_txt;

// byte sequence, same as opencv.
enum {color_gray, color_rgb, color_bgr};

struct tparams
{
	tparams(int width, int height, int color)
		: width(width)
		, height(height)
		, color(color)
	{}

	int width;
	int height;
	int color;
};

struct tsession {
	std::string key;
	std::unique_ptr<tfile> fp;
	std::unique_ptr<tflite::FlatBufferModel> model;
	std::unique_ptr<tflite::Interpreter> interpreter;

	bool valid() const { return fp.get() && model.get() && interpreter.get(); }
	void reset() 
	{
		// it doesn't touch key.
		fp.reset(nullptr);
		model.reset(nullptr);
		interpreter.reset(nullptr);
	}
};

bool is_valid_tflite(const std::string& file);
bool is_valid_tflits(const std::string& file, const std::string& sha256);
bool load_model(const std::string& file, tsession& session, const std::string& sha256);
std::map<std::string, tocr_result> ocr(const config& app_cfg, CVideo& video, const surface& surf, const std::vector<std::string>& fields, const std::string& pb_short_path, const std::string& sha256, const tflite::tparams& tflite_params, const bool bg_color_partition);

std::vector<std::string> load_labels_txt(const std::string& path);
std::set<wchar_t> load_labels_txt_unicode(const std::string& path);

uint32_t invoke_classifier(Interpreter& interpreter, const int output_size, const int N, const float kThreshold, std::vector<std::pair<float, int> >& top_results);
wchar_t inference_char(tsession& session, const std::string& tflite_file, const std::string& sha256, cv::Mat& src, uint32_t* used_ticks);

std::string sha256(const std::string& private_key, const std::string& appid);
#pragma pack(4)
struct ttflits_header {
	uint64_t timestamp;
	uint32_t fourcc;
	uint32_t reserve;
};
#pragma pack()
void encrypt_tflite(const std::string& src_path, const std::string& dst_path, const std::string& sha256);
bool decrypt_tflite(uint8_t* fdata, int64_t fsize, const std::string& sha256);

}


#endif
