#ifndef UNIT_MAP_HPP_INCLUDED
#define UNIT_MAP_HPP_INCLUDED

#include "ocr_unit.hpp"
#include "base_map.hpp"

class ocr_controller;

class ocr_unit_map: public base_map
{
public:
	ocr_unit_map(ocr_controller& controller, const tmap& gmap, bool consistent);

	void add(const map_location& loc, const base_unit* base_u);

	ocr_unit* find_unit(const map_location& loc) const;
	ocr_unit* find_unit(const map_location& loc, bool verlay) const;
	ocr_unit* find_unit(int i) const { return dynamic_cast<ocr_unit*>(map_[i]); }
};

#endif
