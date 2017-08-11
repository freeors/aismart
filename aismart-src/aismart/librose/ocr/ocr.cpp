#define GETTEXT_DOMAIN "rose-lib"

#include "ocr/ocr.hpp"
#include "serialization/string_utils.hpp"
#include "sdl_utils.hpp"
#include "wml_exception.hpp"

void tocr_line::calculate_bounding_rect()
{
	VALIDATE(!chars.empty(), null_str);

	bounding_rect = create_rect(INT_MAX, INT_MAX, -1, -1);
	int valid_chars = 0;
	int blank_y = 0, blank_h = 0, blanks = 0;

	for (std::map<trect, int>::const_iterator it = chars.begin(); it != chars.end(); ++ it) {
		const trect& rect = it->first;
		const int threshold = it->second;

		if (rect.x < bounding_rect.x) {
			bounding_rect.x = rect.x;
		}

		if (rect.x + rect.w > bounding_rect.w) {
			bounding_rect.w = rect.x + rect.w;
		}

		if (threshold >= 0 && threshold <= 255) {
			if (rect.y < bounding_rect.y) {
				bounding_rect.y = rect.y;
			}
			if (rect.y + rect.h > bounding_rect.h) {
				bounding_rect.h = rect.y + rect.h;
			}
			valid_chars ++;

		} else if (threshold == -1) {
			blank_y += rect.y;
			blank_h += rect.h;
			blanks ++;
		}
	}

	bounding_rect.w -= bounding_rect.x;
	if (valid_chars) {
		bounding_rect.h -= bounding_rect.y;
	} else {
		VALIDATE(blanks > 0, null_str);
		bounding_rect.y = blank_y / blanks;
		bounding_rect.h = blank_h / blanks;
	}
}

