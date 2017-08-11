#ifndef UNIT_HPP_INCLUDED
#define UNIT_HPP_INCLUDED

#include "base_unit.hpp"
#include "ocr/ocr.hpp"

class ocr_controller;
class ocr_display;
class ocr_unit_map;

class ocr_unit: public base_unit
{
public:
	ocr_unit(ocr_controller& controller, ocr_display& disp, ocr_unit_map& units, const tocr_line& line);

	tocr_line& line() { return line_; }
	const tocr_line& line() const { return line_; }

private:
	void app_draw_unit(const int xsrc, const int ysrc) override;

protected:
	ocr_controller& controller_;
	ocr_display& disp_;
	ocr_unit_map& units_;
	tocr_line line_;
};

#endif
