#define GETTEXT_DOMAIN "ocr-lib"

#include "ocr_unit_map.hpp"
#include "ocr_display.hpp"
#include "ocr_controller.hpp"
#include "gui/dialogs/ocr_scene.hpp"

ocr_unit_map::ocr_unit_map(ocr_controller& controller, const tmap& gmap, bool consistent)
	: base_map(controller, gmap, consistent)
{
}

void ocr_unit_map::add(const map_location& loc, const base_unit* base_u)
{
	const ocr_unit* u = dynamic_cast<const ocr_unit*>(base_u);
	insert(loc, new ocr_unit(*u));
}

ocr_unit* ocr_unit_map::find_unit(const map_location& loc) const
{
	return dynamic_cast<ocr_unit*>(find_base_unit(loc));
}

ocr_unit* ocr_unit_map::find_unit(const map_location& loc, bool overlay) const
{
	return dynamic_cast<ocr_unit*>(find_base_unit(loc, overlay));
}