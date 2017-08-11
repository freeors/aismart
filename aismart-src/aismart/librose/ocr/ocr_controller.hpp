#ifndef OCR_CONTROLLER_HPP_INCLUDED
#define OCR_CONTROLLER_HPP_INCLUDED

#include "base_controller.hpp"
#include "mouse_handler_base.hpp"
#include "ocr_display.hpp"
#include "ocr_unit_map.hpp"
#include "map.hpp"

#include "tflite.hpp"

class ocr_controller : public base_controller, public events::mouse_handler_base
{
public:
	ocr_controller(const config &app_cfg, CVideo& video, surface& surf, std::vector<std::unique_ptr<tocr_line> >& lines, const std::vector<std::string>& fields, const std::string& tflite, const std::string& sha256, const tflite::tparams& tflite_params);
	~ocr_controller();

	const std::vector<std::string>& fields() const { return fields_; }
	const std::vector<std::unique_ptr<tocr_line> >& lines() const { return lines_; }

	void start_drag(int field_at)
	{
		VALIDATE(dragging_field_ == nposm, null_str);
		dragging_field_ = field_at;
	}
	int dragging_field() const { return dragging_field_; }
	const std::pair<ocr_unit*, int>& adjusting_line() const { return adjusting_line_; }

	bool did_drag_mouse_motion(const int x, const int y, gui2::twindow& window);
	void did_drag_mouse_leave(const int x, const int y, bool except_leave);

	bool field_is_bound(const std::string& field) const;

	const std::map<std::string, tocr_result>& recognize_result() const { return recognize_result_; }

private:
	void app_create_display(int initial_zoom) override;
	void app_post_initialize() override;

	bool app_mouse_motion(const int x, const int y, const bool minimap) override;
	void app_left_mouse_down(const int x, const int y, const bool minimap) override;
	void app_mouse_leave_main_map(const int x, const int y, const bool minimap) override;

	void app_execute_command(int command, const std::string& sparam) override;

	ocr_display& gui() { return *gui_; }
	const ocr_display& gui() const { return *gui_; }
	events::mouse_handler_base& get_mouse_handler_base() override { return *this; }
	ocr_display& get_display() override { return *gui_; }
	const ocr_display& get_display() const override { return *gui_; }

	ocr_unit_map& get_units() override { return units_; }
	const ocr_unit_map& get_units() const override { return units_; }

private:
	void bind_field_2_unit(int dragging_field_, ocr_unit& u);
	void recognize();
	bool did_recognize(gui2::tprogress_& progress);

private:
	ocr_unit_map units_;
	tmap map_;
	ocr_display* gui_;
	const surface& target_;

	const std::vector<std::unique_ptr<tocr_line> >& lines_;
	const std::vector<std::string>& fields_;
	const std::string pb_path_;
	const std::string sha256_;

	int dragging_field_;
	std::pair<ocr_unit*, int> adjusting_line_;
	tpoint start_adjusting_xy_;

	const tflite::tparams tflite_params_;

	tflite::tsession current_session_;
	std::map<std::string, tocr_result> recognize_result_;
};

#endif
