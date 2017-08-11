#ifndef LIBROSE_OCR_OCR_HPP_INCLUDED
#define LIBROSE_OCR_OCR_HPP_INCLUDED

#include "util.hpp"
#include "config.hpp"
#include <set>

class tocr_line 
{
public:
	tocr_line(const std::map<trect, int>& _chars)
		: chars(_chars)
		, field()
		, bounding_rect{0, 0, 0, 0}
	{}

	void calculate_bounding_rect();

	SDL_Rect bounding_rect;
	std::map<trect, int> chars;
	std::string field; // coresponding field
};

struct tocr_result 
{
	explicit tocr_result(const std::string& chars, uint32_t used_ms)
		: chars(chars)
		, used_ms(used_ms)
	{}

	std::string chars;
	uint32_t used_ms;
};

#endif

