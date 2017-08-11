#ifndef LIBROSE_GUI_DIALOGS_OCR_HPP_INCLUDED
#define LIBROSE_GUI_DIALOGS_OCR_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "ocr/ocr.hpp"
#include "rtc_client.hpp"

namespace gui2 {

class ttrack;
class tbutton;

class tbboxes
{
public:
	tbboxes()
		: cache(nullptr)
		, cache_size_(0)
		, cache_vsize(0)
	{}
	~tbboxes()
	{
		if (cache) {
			free(cache);
		}
	}

	SDL_Rect to_rect(int at) const;

	void insert(const cv::Rect& rect);
	void erase(const SDL_Rect& rect);
	int find(const uint64_t key) const;
	int inserting_at(const uint64_t key) const;
	bool is_character(const SDL_Rect& char_rect) const;

private:
	void resize_cache(int size);

public:
	uint64_t* cache;
	int cache_vsize;

private:
	int cache_size_;
};

class tocr: public tdialog
{
public:
	explicit tocr(const surface& target, std::vector<std::unique_ptr<tocr_line> >& lines, const bool bg_color_partition);

	~tocr();
	surface target(tpoint& offset) const;

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

private:
	void did_draw_paper(ttrack& widget, const SDL_Rect& widget_rect, const bool bg_drawn);
	void did_mouse_leave_paper(ttrack& widget, const tpoint&, const tpoint& /*last_coordinate*/);
	void did_mouse_motion_paper(ttrack& widget, const tpoint& first, const tpoint& last);
	void save(twindow& window);

	void locate_lines(surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines);
	void locate_lines_epoch(surface& surf, const cv::Mat& src, const cv::Mat& grey, tbboxes& bboxes3, std::vector<std::unique_ptr<tocr_line> >& lines, const int epoch);

	void start_avcapture();
	void stop_avcapture();
	void did_paper_draw_slice(bool remote, const cv::Mat& frame, const texture& tex, const SDL_Rect& draw_rect, bool new_frame, texture& cv_tex);

	SDL_Rect calculate_thumbnail_rect(const int xsrc, const int ysrc, const int frame_width, const int frame_height);

	class texecutor: public tworker
	{
	public:
		texecutor(tocr& ocr)
			: ocr_(ocr)
		{
			thread_->Start();
		}

	private:
		void DoWork() override;
		void OnWorkStart() override {}
		void OnWorkDone() override {}

	private:
		tocr& ocr_;
	};

private:
	const surface target_;
	std::vector<std::unique_ptr<tocr_line> >& lines_;
	const bool bg_color_partition_;
	bool verbose_;

	ttrack* paper_;
	tbutton* save_;

	class tsetting_lock
	{
	public:
		tsetting_lock(tocr& ocr)
			: ocr_(ocr)
		{
			VALIDATE(!ocr_.setting_, null_str);
			ocr_.setting_ = true;
		}
		~tsetting_lock()
		{
			ocr_.setting_ = false;
		}

	private:
		tocr& ocr_;
	};
	bool setting_;
	bool recognition_thread_running_;

	std::pair<bool, surface> current_surf_; // first: valid. now can recognition.
	std::unique_ptr<tavcapture> avcapture_;

	threading::mutex variable_mutex_;
	threading::mutex recognition_mutex_;

	bool cv_tex_pixel_dirty_;
	uint8_t* blended_tex_pixels_;
	surface camera_surf_;

	std::unique_ptr<texecutor> executor_;
	tpoint thumbnail_size_;
	tpoint original_local_offset_;
	tpoint current_local_offset_;
	tpoint current_render_size_;

	int next_recognize_frame_;
};

} // namespace gui2

#endif

