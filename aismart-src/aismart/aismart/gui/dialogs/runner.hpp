#ifndef GUI_DIALOGS_RUNNER_HPP_INCLUDED
#define GUI_DIALOGS_RUNNER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "rtc_client.hpp"
#include "tflite.hpp"
#include "game_config.hpp"

#include <opencv2/core.hpp>


namespace gui2 {

class ttoggle_button;
class ttrack;

class trunner: public tdialog, public tworker
{
public:
	enum tresult {OCR = 1, CHAT};
	enum {BASE_LAYER, OCR_LAYER, MORE_LAYER};

	explicit trunner(const tscript& script, const ttflite& tflite);
	~trunner();

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void app_first_drawn() override;

	void DoWork() override;
	void OnWorkStart() override;
	void OnWorkDone() override;

	// base
	void pre_base(twindow& window);

	void insert_blits(std::vector<image::tblit>& blits, const surface& surf, const std::string& result, std::vector<std::pair<float, SDL_Rect> >& rects);

	void classifier_image();
	void pr_image();

	void did_draw_paper(ttrack& widget, const SDL_Rect& draw_rect, const bool bg_drawn);
	void did_left_button_down_paper(ttrack& widget, const tpoint& coordinate);
	void did_mouse_leave_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);

	std::string classifier_surface(const surface& surf);
	std::string example_detector_internal(surface& surf);
	std::string pr_surface(surface& surf);

	void did_paper_draw_slice(bool remote, const cv::Mat& frame, const texture& tex, const SDL_Rect& draw_rect, bool new_frame);

	void stop_avcapture();
	void start_avcapture();

private:
	const tscript& script_;
	const ttflite& tflite_;

	ttrack* paper_;
	std::vector<image::tblit> blits_;

	tflite::tsession current_session_;
	std::pair<bool, surface> current_surf_; // first: valid. now can recognition.

	threading::mutex recognition_mutex_;
	threading::mutex variable_mutex_;
	bool recognition_thread_running_;

	std::string result_;
	std::vector<std::pair<float, SDL_Rect> > rects_;
	std::vector<std::string> label_strings_;
	std::vector<float> locations_;

	class tsetting_lock
	{
	public:
		tsetting_lock(trunner& home)
			: home_(home)
		{
			VALIDATE(!home_.setting_, null_str);
			home_.setting_ = true;
		}
		~tsetting_lock()
		{
			home_.setting_ = false;
		}

	private:
		trunner& home_;
	};
	bool setting_;

	std::unique_ptr<tavcapture> avcapture_;
	surface persist_surf_;
	surface temperate_surf_;

	int next_recognize_frame_;

	uint32_t start_ticks_;
	tpoint last_coordinate_;
	cv::RNG rng_;
};

} // namespace gui2

#endif

