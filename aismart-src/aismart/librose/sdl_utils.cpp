/* $Id: sdl_utils.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Support-routines for the SDL-graphics-library.
 */

#include "global.hpp"

#include "sdl_image.h"
#include "sdl_utils.hpp"
#include "video.hpp"
#include "image.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>

#include <opencv2/imgproc/imgproc.hpp>


const SDL_Rect empty_rect = {0, 0, 0, 0};

// as if valid rect, x or y maybe negative.
const SDL_Rect null_rect = {0, 0, -1, -1};

void surface::sdl_add_ref(SDL_Surface *surf)
{
	if (surf != NULL) {
		++surf->refcount;
	}
}

surface::surface(const cv::Mat& mat)
	: surface_(nullptr)
	, mat_(nullptr)
{
	const int channels = mat.channels();
	VALIDATE(mat.cols >0 && mat.rows > 0 && (channels == 3 || channels == 4) && mat.u, null_str);

	if (channels == 4) {
		surface_ = SDL_CreateRGBSurfaceFrom(mat.data, mat.cols, mat.rows, channels * 8, channels * mat.cols, 0xFF0000, 0xFF00, 0xFF, 0xFF000000);
	} else {
		surface_ = SDL_CreateRGBSurfaceFrom(mat.data, mat.cols, mat.rows, channels * 8, channels * mat.cols, 
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			0xFF0000, 0xFF00, 0xFF,
#else
			0xFF, 0xFF00, 0xFF0000,
#endif
			0x0000000);
	}

	mat_ = new cv::Mat;
	*mat_ = mat;
	SDL_SetSurfaceRLE(surface_, 0);
}

surface::~surface()
{
	free_sdl_surface();
}

void surface::free_sdl_surface()
{
	if (surface_ != nullptr) {
		if (!mat_ || surface_->refcount > 1) {
			SDL_FreeSurface(surface_);
		} else {
			VALIDATE(surface_->refcount == 1, null_str);
			surface_->flags |= SDL_PREALLOC;
			SDL_FreeSurface(surface_);
			delete mat_;
			mat_ = nullptr;
		}
		surface_ = nullptr;
	} else {
		VALIDATE(!mat_, null_str);
	}
}

void surface::assign2(SDL_Surface* surf, cv::Mat* mat) 
{
	free_sdl_surface();
	surface_ = surf;
	mat_ = mat;
}

void render_line(SDL_Renderer* renderer, Uint32 argb, int x1, int y1, int x2, int y2)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void render_rect_frame(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderDrawRect(renderer, &rect);
}

void render_rect(SDL_Renderer* renderer, const SDL_Rect& rect, Uint32 argb)
{
	SDL_Color color = uint32_to_color(argb);
	trender_draw_color_lock lock(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

// Check that the surface is neutral bpp 32, possibly with an empty alpha channel.
bool is_neutral_surface(const surface& surf)
{
	const SDL_PixelFormat* format = surf->format;
	// don't only use (format->BitsPerPixel != 32), it will result access invalidate in uncopy_32(SDL)!
	return SDL_BITSPERPIXEL(format->format) == 32 && (format->BytesPerPixel == 4 && format->Rmask == 0xFF0000u && (format->Amask | 0xFF000000u) == 0xFF000000u);
}

surface_lock::surface_lock(surface &surf) : surface_(surf), locked_(false)
{
	if (SDL_MUSTLOCK(surface_)) {
		locked_ = SDL_LockSurface(surface_) == 0;
	}
}

surface_lock::~surface_lock()
{
	if (locked_) {
		SDL_UnlockSurface(surface_);
	}
}

const_surface_lock::const_surface_lock(const surface &surf) : surface_(surf), locked_(false)
{
	if (SDL_MUSTLOCK(surface_)) {
		locked_ = SDL_LockSurface(surface_) == 0;
	}
}

const_surface_lock::~const_surface_lock()
{
	if (locked_) {
		SDL_UnlockSurface(surface_);
	}
}

tsurface_2_mat_lock::tsurface_2_mat_lock(const surface& surf) 
	: surface_(surf)
	, locked_(false)
	, mat()
{
	VALIDATE(surf, null_str);

	const int BytesPerPixel = surf->format->BytesPerPixel;
	VALIDATE(BytesPerPixel == 3 || BytesPerPixel == 4, null_str);

	if (SDL_MUSTLOCK(surface_)) {
		VALIDATE(BytesPerPixel == 4, null_str);
		locked_ = SDL_LockSurface(surface_) == 0;
	}
	mat = cv::Mat(surf->h, surf->w, surf->format->BytesPerPixel == 4? CV_8UC4: CV_8UC3, surf->pixels);
}

tsurface_2_mat_lock::~tsurface_2_mat_lock()
{
	if (locked_) {
		SDL_UnlockSurface(surface_);
	}
}

ttexture_2_mat_lock::ttexture_2_mat_lock(texture& tex) 
	: texture_(tex)
	, mat()
{
	uint32_t format;
	int access, width, height;
	SDL_QueryTexture(tex.get(), &format, &access, &width, &height);
	VALIDATE(access == SDL_TEXTUREACCESS_STREAMING, null_str);

	uint8_t* pixels = nullptr;
	int pitch = 0;
	SDL_LockTexture(tex.get(), NULL, (void**)&pixels, &pitch);
	VALIDATE(pitch == width * 4, null_str);
	mat = cv::Mat(height, width, CV_8UC4, pixels);
}

struct RGB2Gray
{
	enum {
		yuv_shift = 14,
		xyz_shift = 12,
		R2Y = 4899, // == R2YF*16384
		G2Y = 9617, // == G2YF*16384
		B2Y = 1868, // == B2YF*16384
		BLOCK_SIZE = 256
	};

    RGB2Gray(int _srccn, int blueIdx, const int* coeffs) : srccn(_srccn)
    {
        const int coeffs0[] = { R2Y, G2Y, B2Y };
        if(!coeffs) coeffs = coeffs0;

        int b = 0, g = 0, r = (1 << (yuv_shift-1));
        int db = coeffs[blueIdx^2], dg = coeffs[1], dr = coeffs[blueIdx];

		if (tab[0] == 0 && tab[256] == 0 && tab[512] == r && tab[1] == db && tab[257] == dg && tab[513] == r + dr) {
			return;
		}
        for( int i = 0; i < 256; i++, b += db, g += dg, r += dr )
        {
            tab[i] = b;
            tab[i+256] = g;
            tab[i+512] = r;
        }
    }
    void translate(const uchar* src, uchar* dst, int n) const
    {
        int scn = srccn;
        const int* _tab = tab;
        for(int i = 0; i < n; i++, src += scn)
            dst[i] = (uchar)((_tab[src[0]] + _tab[src[1]+256] + _tab[src[2]+512]) >> yuv_shift);
    }
    int srccn;
	int tab[256*3];
};

struct Gray2RGB
{
    Gray2RGB(int _dstcn) : dstcn(_dstcn) {}
    void translate(const uchar* src, uchar* dst, int n) const
    {
        if( dstcn == 3 )
            for( int i = 0; i < n; i++, dst += 3 )
            {
                dst[0] = dst[1] = dst[2] = src[i];
            }
        else
        {
            for( int i = 0; i < n; i++, dst += 4 )
            {
                dst[0] = dst[1] = dst[2] = src[i];
                dst[3] = 255;
            }
        }
    }

    int dstcn;
};

tframe_ticks frame_ticks = {0, 0, 0, 0, 0};

void cvtColor2(const cv::Mat& _src, cv::Mat& _dst, int code)
{
	VALIDATE(code == cv::COLOR_BGRA2GRAY || code == cv::COLOR_GRAY2BGRA, null_str);

	const uint32_t start = SDL_GetTicks();
	if (code == cv::COLOR_BGRA2GRAY) {
		VALIDATE(_src.type() == CV_8UC4, null_str);

		_dst.create(_src.size(), CV_8UC1);
		RGB2Gray translate(4, 0, nullptr);
		translate.translate(_src.data, _dst.data, _dst.rows * _dst.cols);

	} else if (code == cv::COLOR_GRAY2BGRA) {
		VALIDATE(_src.type() == CV_8UC1, null_str);

		_dst.create(_src.size(), CV_8UC4);
		Gray2RGB translate(4);
		translate.translate(_src.data, _dst.data, _src.rows * _src.cols);

	}

	const uint32_t mid = SDL_GetTicks();
	cv::cvtColor(_src, _dst, code);

	uint32_t end = SDL_GetTicks();

	frame_ticks.frames ++;
	frame_ticks.task1 += mid - start;
	frame_ticks.task2 += end - mid;

	if (!(frame_ticks.frames % 100)) {
		SDL_Log("frames(%i), rose(%7.2f), cv(%7.2f)\n", frame_ticks.frames, 
			1.0 * frame_ticks.task1 / frame_ticks.frames,
			1.0 * frame_ticks.task2 / frame_ticks.frames);
	}
}

uint32_t decode_color(const std::string& color)
{
	if (color.empty()) {
		return 0;
	}

	if (color.find(',') == std::string::npos) {
		const char* c_str = color.c_str();	
		if (c_str[0] == '(') {
			return FORMULA_COLOR;
		} else if (c_str[0] >= '0' && c_str[0] <= '9') {
			return PREDEFINE_COLOR + atoi(c_str);
		}
		VALIDATE(false, null_str);
	}

	std::vector<std::string> fields = utils::split(color);
	// make sure we have four fields
	while (fields.size() < 4) fields.push_back("0");

	uint32_t result = 0;
	for (int i = 0; i < 4; ++i) {
		// shift the previous value before adding, since it's a nop on the
		// first run there's no need for an if.
		result = result << 8;
		// result |= lexical_cast_default<int>(fields[i]);
		result |= utils::to_int(fields[i]);
	}
	if (!(result & 0xff000000)) {
		// avoid to confuse with special color.
		result = 0;
	}
	return result;
}

std::string encode_color(const uint32_t argb)
{
	SDL_Color result = uint32_to_color(argb);
	std::stringstream ss;
	ss << (int)result.a << ", " << (int)result.r << ", " << (int)result.g << ", " << (int)result.b;
	return ss.str();
}

SDL_Color uint32_to_color(const Uint32 argb)
{
	SDL_Color result;
	result.r = (0x00FF0000 & argb) >> 16;
	result.g = (0x0000FF00 & argb) >> 8;
	result.b = (0x000000FF & argb);
	result.a = (0xFF000000 & argb) >> 24;
	return result;
}

uint32_t color_to_uint32(const SDL_Color& color)
{
	return color.a << 24 | color.r << 16 | color.g << 8 | color.b;
}

SDL_Color create_color(const unsigned char red
		, unsigned char green
		, unsigned char blue
		, unsigned char alpha)
{
	SDL_Color result;
	result.r = red;
	result.g = green;
	result.b = blue;
	result.a = alpha;

	return result;
}

void put_pixel(
		  const ptrdiff_t start
		, const Uint32 color
		, const unsigned w
		, const unsigned x
		, const unsigned y)
{
	*reinterpret_cast<Uint32*>(start + (y * w * 4) + x * 4) = color;
}

// only condition: x1 >= x2.
void draw_line(
		  surface& canvas
		, Uint32 color
		, int x1
		, int y1
		, int x2
		, int y2)
{
	ptrdiff_t start = reinterpret_cast<ptrdiff_t>(canvas->pixels);
	int w = canvas->w;
	int h = canvas->h;

	VALIDATE(x2 >= x1, null_str);
	if (x1 >= w) {
		return;
	}
	if (x2 >= w) {
		x2 = w - 1;
	}

	// use a special case for vertical lines
	if (x1 == x2) {
		if (y2 < y1) {
			std::swap(y1, y2);
		}

		if (y1 >= h) {
			return;
		}
		if (y2 >= h) {
			y2 = h - 1;
		}
		for (int y = y1; y <= y2; ++ y) {
			put_pixel(start, color, w, x1, y);
		}
		return;
	}

	// use a special case for horizontal lines
	if (y1 == y2) {
		for (int x = x1; x <= x2; ++ x) {
			put_pixel(start, color, w, x, y1);
		}
		return;
	}

	// Algorithm based on
	// http://de.wikipedia.org/wiki/Bresenham-Algorithmus#Kompakte_Variante
	// version of 26.12.2010.
	const int dx = x2 - x1; // precondition x2 >= x1
	const int dy = abs(y2 - y1);
	const int step_x = 1;
	const int step_y = y1 < y2 ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2;
	int e2;

	for (;;) {
		if (x1 > 0 && x1 < w && y1 > 0 && y1 < h) {
			put_pixel(start, color, w, x1, y1);
		}
		if (x1 == x2 && y1 == y2) {
			break;
		}
		e2 = err;
		if (e2 > -dx) {
			err -= dy;
			x1 += step_x;
		}
		if (e2 <  dy) {
			err += dx;
			y1 += step_y;
		}
	}
}

void draw_circle(
		  surface& canvas
		, Uint32 color
		, const unsigned x_center
		, const unsigned y_center
		, const unsigned radius
		, bool require_map)
{
	if (require_map) {
		color = SDL_MapRGBA(canvas->format,
			((color & 0xFF000000) >> 24),
			((color & 0x00FF0000) >> 16),
			((color & 0x0000FF00) >> 8),
			((color & 0x000000FF)));
	}

	ptrdiff_t start = reinterpret_cast<ptrdiff_t>(canvas->pixels);
	unsigned w = canvas->w;

	VALIDATE(static_cast<int>(x_center + radius) < canvas->w, null_str);
	VALIDATE(static_cast<int>(x_center - radius) >= 0, null_str);
	VALIDATE(static_cast<int>(y_center + radius) < canvas->h, null_str);
	VALIDATE(static_cast<int>(y_center - radius) >= 0, null_str);

	// Algorithm based on
	// http://de.wikipedia.org/wiki/Rasterung_von_Kreisen#Methode_von_Horn
	// version of 2011.02.07.
	int d = -static_cast<int>(radius);
	int x = radius;
	int y = 0;
	while (!(y > x)) {
		put_pixel(start, color, w, x_center + x, y_center + y);
		put_pixel(start, color, w, x_center + x, y_center - y);
		put_pixel(start, color, w, x_center - x, y_center + y);
		put_pixel(start, color, w, x_center - x, y_center - y);

		put_pixel(start, color, w, x_center + y, y_center + x);
		put_pixel(start, color, w, x_center + y, y_center - x);
		put_pixel(start, color, w, x_center - y, y_center + x);
		put_pixel(start, color, w, x_center - y, y_center - x);

		d += 2 * y + 1;
		++ y;
		if(d > 0) {
			d += -2 * x + 2;
			--x;
		}
	}
}

SDL_Keycode sdl_keysym_from_name(std::string const &keyname)
{
	static bool initialized = false;
	typedef std::map<std::string const, SDL_Keycode> keysym_map_t;
	static keysym_map_t keysym_map;

	if (!initialized) {
		// for (SDL_Keycode i = SDLK_FIRST; i < SDLK_LAST; i = SDL_Keycode(int(i) + 1)) {
		for (SDL_Keycode i = 0; i < 256; i = SDL_Keycode(int(i) + 1)) {
			std::string name = SDL_GetKeyName(i);
			if (!name.empty())
				keysym_map[name] = i;
		}
		initialized = true;
	}

	keysym_map_t::const_iterator it = keysym_map.find(keyname);
	if (it != keysym_map.end())
		return it->second;
	else
		return SDLK_UNKNOWN;
}

bool point_in_rect(int x, int y, const SDL_Rect& rect)
{
	return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

bool rects_overlap(const SDL_Rect& rect1, const SDL_Rect& rect2)
{
	return (rect1.x < rect2.x + rect2.w && rect2.x < rect1.x + rect1.w &&
			rect1.y < rect2.y + rect2.h && rect2.y < rect1.y + rect1.h);
}

SDL_Rect intersect_rects(SDL_Rect const &rect1, SDL_Rect const &rect2)
{
	SDL_Rect res;
	res.x = std::max<int>(rect1.x, rect2.x);
	res.y = std::max<int>(rect1.y, rect2.y);
	int w = std::min<int>(rect1.x + rect1.w, rect2.x + rect2.w) - res.x;
	int h = std::min<int>(rect1.y + rect1.h, rect2.y + rect2.h) - res.y;
	if (w <= 0 || h <= 0) return empty_rect;
	res.w = w;
	res.h = h;
	return res;
}

SDL_Rect union_rects(SDL_Rect const &rect1, SDL_Rect const &rect2)
{
	if (rect1.w == 0 || rect1.h == 0) return rect2;
	if (rect2.w == 0 || rect2.h == 0) return rect1;
	SDL_Rect res;
	res.x = std::min<int>(rect1.x, rect2.x);
	res.y = std::min<int>(rect1.y, rect2.y);
	res.w = std::max<int>(rect1.x + rect1.w, rect2.x + rect2.w) - res.x;
	res.h = std::max<int>(rect1.y + rect1.h, rect2.y + rect2.h) - res.y;
	return res;
}

SDL_Rect create_rect(const int x, const int y, const int w, const int h)
{
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	return rect;
}

SDL_Point create_point(const int x, const int y)
{
	SDL_Point pt;
	pt.x = x;
	pt.y = y;
	return pt;
}

bool operator<(const surface& a, const surface& b)
{
	return a.get() < b.get();
}

const SDL_PixelFormat& get_neutral_pixel_format()
{
	static bool first_time = true;
	static SDL_PixelFormat format;

	if (first_time) {
		first_time = false;
		surface surf(SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000));
		format = *surf->format;
		format.palette = NULL;
	}

	return format;
}

surface clone_surface(const surface &surf)
{
	if (surf == NULL) {
		return NULL;
	}

	// it will create new surface, don't modify surf.
	surface const result = SDL_ConvertSurface(surf, &get_neutral_pixel_format(), SDL_SWSURFACE);
	VALIDATE(result, null_str);
	SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);

	return result;
}

surface create_neutral_surface(int w, int h)
{
	if (w < 0 || h < 0) {
		// error : neutral surface with negative dimensions
		return NULL;
	}

	const SDL_PixelFormat format = get_neutral_pixel_format();
	surface result = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
			format.BitsPerPixel,
			format.Rmask,
			format.Gmask,
			format.Bmask,
			format.Amask);

	VALIDATE(result, null_str);

	SDL_BlendMode mode;
	SDL_GetSurfaceBlendMode(result, &mode);
	VALIDATE(mode == SDL_BLENDMODE_BLEND, null_str);

	return result;
}

texture create_neutral_texture(const int w, const int h, const int access)
{
	VALIDATE(w > 0 && h > 0, null_str);

	const SDL_PixelFormat format = get_neutral_pixel_format();
	texture result = SDL_CreateTexture(get_renderer(), format.format, access, w, h);
	VALIDATE(result, null_str);

	SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND);
	return result;
}

surface makesure_neutral_surface(const surface &surf)
{
	if (surf == NULL) {
		return NULL;
	}

	surface result;
	const SDL_PixelFormat* format = surf->format;
	if (!is_neutral_surface(surf)) {
		// VALIDATE(false, null_str); // I hope app make sure this.
		result = clone_surface(surf);
		VALIDATE(result, null_str);
	} else {
		result = surf;
	}

    SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
	return result;
}

void fill_surface(surface& surf, uint32_t color)
{
	VALIDATE(surf && is_neutral_surface(surf), null_str);

	{
		surface_lock lock(surf);

		Uint32* beg = lock.pixels();
		Uint32* end = beg + surf->w * surf->h;

		while (beg != end) {
			*beg = color;
			++beg;
		}
	}
}

SDL_Rect calculate_max_foreground_region(const surface& surf, uint32_t background)
{
	VALIDATE(surf.get(), null_str);
	SDL_Rect ret{ INT_MAX, INT_MAX, -1, -1};

	const_surface_lock lock(surf);

	int w = surf->w;
	int h = surf->h;

	for (int row = 0; row < h; row ++) {
		const uint32_t* data = lock.pixels() + row * w;
		for (int col = 0; col < w; col ++) {
			uint32_t value = data[col];
			if (value != background) {
				if (col < ret.x) {
					ret.x = col;
				}
				if (col > ret.w) {
					ret.w = col;
				}
				if (row < ret.y) {
					ret.y = row;
				}
				if (row > ret.h) {
					ret.h = row;
				}
			}
		}
	}

	if (ret.w != -1) {
		VALIDATE(ret.h != -1, null_str);
		ret.w = ret.w - ret.x + 1;
		ret.h = ret.h - ret.y + 1;
	}
	return ret;
}

surface stretch_surface_horizontal(const surface& surf, const unsigned w, int pixels)
{
	if (surf == NULL)
		return NULL;

	if (static_cast<int>(w) == surf->w) {
		return surf;
	}
	VALIDATE(w > 0 && is_neutral_surface(surf), null_str);

	surface dst(create_neutral_surface(w, surf->h));
	if (dst == NULL) {
		std::cerr << "Could not create surface to scale onto\n";
		return NULL;
	}

	{
		// Extra scoping used for the surface_lock.
		const_surface_lock src_lock(surf);
		surface_lock dst_lock(dst);

		const Uint32* const src_pixels = src_lock.pixels();
		Uint32* dst_pixels = dst_lock.pixels();

		for(unsigned y = 0; y < static_cast<unsigned>(surf->h); ++y) {
			const Uint32 pixel = src_pixels [y * surf->w];
			for(unsigned x = 0; x < w; ++x) {

				*dst_pixels++ = pixel;

			}
		}
	}

	return dst;
}

surface stretch_surface_vertical(const surface& surf, const unsigned h, int pixels)
{
	if (surf == NULL)
		return NULL;

	if (static_cast<int>(h) == surf->h) {
		return surf;
	}
	VALIDATE(h > 0 && is_neutral_surface(surf), null_str);

	surface dst(create_neutral_surface(surf->w, h));
	if (dst == NULL) {
		std::cerr << "Could not create surface to scale onto\n";
		return NULL;
	}

	{
		// Extra scoping used for the surface_lock.
		const_surface_lock src_lock(surf);
		surface_lock dst_lock(dst);

		const Uint32* const src_pixels = src_lock.pixels();
		Uint32* dst_pixels = dst_lock.pixels();

		for(unsigned y = 0; y < static_cast<unsigned>(h); ++y) {
		  for(unsigned x = 0; x < static_cast<unsigned>(surf->w); ++x) {

				*dst_pixels++ = src_pixels[x];
			}
		}
	}

	return dst;
}

tpoint calculate_adaption_ratio_size(const int outer_w, const int outer_h, const int inner_w, const int inner_h)
{
	VALIDATE(outer_w > 0 && outer_h > 0 && inner_w > 0 && inner_h > 0, null_str);

	double wratio = outer_w * 1.0 / inner_w;
	double hratio = outer_h * 1.0 / inner_h;
	double ratio = wratio> hratio? hratio: wratio;

	tpoint result(static_cast<int>(inner_w * ratio), static_cast<int>(inner_h * ratio));
	if (result.x > outer_w) {
		result.x = outer_w;
	}
	if (result.y > outer_h) {
		result.y = outer_h;
	}
	return result;
}

surface get_adaption_ratio_surface(const surface& src, const int outer_width, const int outer_height)
{
	if (src->w == outer_width && src->h == outer_height) {
		return src;
	}

	tpoint center = calculate_adaption_ratio_size(outer_width, outer_height, src->w, src->h);
	surface center_surf = src;
	if (src->w != center.x || src->h != center.y) {
		center_surf = scale_surface(src, center.x, center.y);
	}
	surface result = create_neutral_surface(outer_width, outer_height);
	fill_surface(result, 0xffffffff);
	SDL_Rect dst_rect{(outer_width - center.x) / 2, (outer_height - center.y) / 2, center_surf->w, center_surf->h};
	sdl_blit(center_surf, nullptr, result, &dst_rect);
	return result;
}

cv::Mat get_adaption_ratio_mat(const cv::Mat& src, const int outer_width, const int outer_height)
{
	VALIDATE(src.channels() == 1 && outer_width > 0 && outer_height > 0, null_str);
	if (src.cols == outer_width && src.rows == outer_height) {
		return src;
	}

	tpoint center = calculate_adaption_ratio_size(outer_width, outer_height, src.cols, src.rows);
	cv::Mat center_mat = src;
	if (src.cols != center.x || src.rows != center.y) {
		cv::resize(src, center_mat, cvSize(center.x, center.y));
	}
	cv::Mat result(outer_height, outer_width, CV_8UC1, cv::Scalar(255));

	cv::Rect roi((outer_width - center.x) / 2, (outer_height - center.y) / 2, center.x, center.y);
	center_mat.copyTo(result(roi));

	return result;
}

void erase_isolated_pixel(cv::Mat& src, const int font_gray, const int background_gray)
{
	VALIDATE(src.channels() == 1, null_str);
	std::unique_ptr<uint8_t> gray_array(new uint8_t[src.rows * src.cols]);
	uint8_t* gray_array_ptr = gray_array.get();

	for (int row = 0; row < src.rows; row ++) {
		const uint8_t* data = src.ptr<uint8_t>(row);
		memcpy(gray_array_ptr + src.cols * row, data, src.cols); 
	}
	// check pixel, if isolated, erase it.
	for (int row = 0; row < src.rows; row ++) {
		const int line_pixel_index = row * src.cols;
		for (int col = 0; col < src.cols; col ++) {
			const int current_index = line_pixel_index + col;
			int gray = gray_array_ptr[current_index];
			if (gray == font_gray) {
				bool twice = false;
				// above line
				if (row > 0) {
					if (col > 0) {
						if (gray_array_ptr[current_index - src.cols - 1] == font_gray) {
							continue;
						}
					}
					if (gray_array_ptr[current_index - src.cols] == font_gray) {
						continue;
					}
					if (col < src.cols - 1) {
						if (gray_array_ptr[current_index - src.cols + 1] == font_gray) {
							continue;
						}
					}
				}
				// this line
				if (col > 0) {
					if (gray_array_ptr[current_index - 1] == font_gray) {
						continue;
					}
				}
				if (col < src.cols - 1) {
					if (gray_array_ptr[current_index + 1] == font_gray) {
						continue;
					}
				}

				// below 1# line
				if (row < src.rows - 1) {
					if (col > 0) {
						if (gray_array_ptr[current_index + src.cols - 1] == font_gray) {
							continue;
						}
					}
					if (col < src.cols - 1) {
						if (gray_array_ptr[current_index + src.cols + 1] == font_gray) {
							continue;
						}
					}
					if (gray_array_ptr[current_index + src.cols] == font_gray) {
						twice = true;
					}
				}
				if (!twice) {
					src.at<uint8_t>(row, col) = background_gray; 
				} else {
					// below 2# line
					if (row < src.rows - 2) {
						if (col > 0) {
							if (gray_array_ptr[current_index + 2 * src.cols - 1] == font_gray) {
								continue;
							}
						}
						if (col < src.cols - 2) {
							if (gray_array_ptr[current_index + 2 * src.cols + 1] == font_gray) {
								continue;
							}
						}
						if (gray_array_ptr[current_index + 2 * src.cols] == font_gray) {
							continue;
						}
					}
					src.at<uint8_t>(row, col) = background_gray; 
					src.at<uint8_t>(row + 1, col) = background_gray; 
				}
			}
		}
	}
}

// NOTE: Don't pass this function 0 scaling arguments.
surface scale_surface(const surface& surf, int w, int h)
{
	if (surf == NULL) {
		return NULL;
	}

	VALIDATE(w >= 0 && h >= 0, null_str);

	if (w == surf->w && h == surf->h) {
		return surf;
	}

	// ---- soft start -----
	tsurface_2_mat_lock mat_lock(surf);
	CvSize czSize(w, h);
	cv::Mat dst_mat;
	cv::resize(mat_lock.mat, dst_mat, cvSize(w, h));

	surface dst(dst_mat);

	// Now both surfaces are always in the "neutral" pixel format
	if (dst == NULL) {
		return NULL;
	}

	return dst;
}

surface scale_surface_blended(const surface &surf, int w, int h, bool optimize)
{
	if (surf== NULL)
		return NULL;

	if (w == surf->w && h == surf->h) {
		return surf;
	}
	VALIDATE(w >= 0 && h >= 0 && is_neutral_surface(surf), null_str);

	surface dst(create_neutral_surface(w,h));
	if (dst == NULL) {
		std::cerr << "Could not create surface to scale onto\n";
		return NULL;
	}

	const double xratio = static_cast<double>(surf->w)/
			              static_cast<double>(w);
	const double yratio = static_cast<double>(surf->h)/
			              static_cast<double>(h);

	{
		const_surface_lock src_lock(surf);
		surface_lock dst_lock(dst);

		const Uint32* const src_pixels = src_lock.pixels();
		Uint32* const dst_pixels = dst_lock.pixels();

		double ysrc = 0.0;
		for(int ydst = 0; ydst != h; ++ydst, ysrc += yratio) {
			double xsrc = 0.0;
			for(int xdst = 0; xdst != w; ++xdst, xsrc += xratio) {
				double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;

				double summation = 0.0;

				// We now have a rectangle, (xsrc,ysrc,xratio,yratio)
				// which we want to derive the pixel from
				for(double xloc = xsrc; xloc < xsrc+xratio; xloc += 1.0) {
					const double xsize = std::min<double>(std::floor(xloc+1.0)-xloc,xsrc+xratio-xloc);
					for(double yloc = ysrc; yloc < ysrc+yratio; yloc += 1.0) {
						const int xsrcint = std::max<int>(0,std::min<int>(surf->w-1,static_cast<int>(xsrc)));
						const int ysrcint = std::max<int>(0,std::min<int>(surf->h-1,static_cast<int>(ysrc)));

						const double ysize = std::min<double>(std::floor(yloc+1.0)-yloc,ysrc+yratio-yloc);

						Uint8 r,g,b,a;

						SDL_GetRGBA(src_pixels[ysrcint*surf->w + xsrcint],surf->format,&r,&g,&b,&a);
						const double value = xsize*ysize*double(a)/255.0;
						summation += value;

						red += r*value;
						green += g*value;
						blue += b*value;
						alpha += a*value;
					}
				}

				if(summation == 0.0)
					summation = 1.0;

				red /= summation;
				green /= summation;
				blue /= summation;
				alpha /= summation;

				dst_pixels[ydst*dst->w + xdst] = SDL_MapRGBA(dst->format,Uint8(red),Uint8(green),Uint8(blue),Uint8(alpha));
			}
		}
	}

	return optimize ? makesure_neutral_surface(dst) : dst;
}

surface rotate_landscape_anticolockwise90(const surface& surf)
{
	if (surf == NULL) {
		return NULL;
	}

	VALIDATE(surf->w >= surf->h, null_str);

	tsurface_2_mat_lock lock(surf);

	const int max_size = surf->w;
	cv::Size new_size{max_size, max_size};
	cv::Point center(max_size / 2, max_size / 2);
	cv::Mat rotMat = cv::getRotationMatrix2D(center, 90, 1);
	cv::Mat rotated;
	cv::warpAffine(lock.mat, rotated, rotMat, new_size);

	cv::Mat img_crop = rotated(cv::Rect(0, 0, surf->h, surf->w)).clone();

	return img_crop;
}

surface adjust_surface_color(const surface &surf, int red, int green, int blue, bool optimize)
{
	if(surf == NULL)
		return NULL;

	if((red == 0 && green == 0 && blue == 0))
		return optimize ? makesure_neutral_surface(surf) : surf;

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "failed to make neutral surface\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg) >> 0;

				r = std::max<int>(0,std::min<int>(255,int(r)+red));
				g = std::max<int>(0,std::min<int>(255,int(g)+green));
				b = std::max<int>(0,std::min<int>(255,int(b)+blue));

				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

void adjust_surface_color2(surface &surf, int red, int green, int blue)
{
	if (!surf || (red == 0 && green == 0 && blue == 0)) {
		return;
	}

	{
		surface_lock lock(surf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + surf->w*surf->h;

		while (beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if (alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg) >> 0;

				r = std::max<int>(0,std::min<int>(255,int(r)+red));
				g = std::max<int>(0,std::min<int>(255,int(g)+green));
				b = std::max<int>(0,std::min<int>(255,int(b)+blue));

				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++ beg;
		}
	}
}

surface greyscale_image(const surface &surf, bool optimize)
{
	if(surf == NULL)
		return NULL;

	surface nsurf(clone_surface(surf));
	if(nsurf == NULL) {
		std::cerr << "failed to make neutral surface\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);
				//const Uint8 avg = (red+green+blue)/3;

				// Use the correct formula for RGB to grayscale conversion.
				// Ok, this is no big deal :)
				// The correct formula being:
				// gray=0.299red+0.587green+0.114blue
				const Uint8 avg = static_cast<Uint8>((
					77  * static_cast<Uint16>(r) +
					150 * static_cast<Uint16>(g) +
					29  * static_cast<Uint16>(b)  ) / 256);

				*beg = (alpha << 24) | (avg << 16) | (avg << 8) | avg;
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface shadow_image(const surface &surf, bool optimize)
{
	if(surf == NULL)
		return NULL;

	// we blur it, and reuse the neutral surface created by the blur function (optimized = false)
	surface nsurf (blur_alpha_surface(surf, 2, false));

	if(nsurf == NULL) {
		std::cerr << "failed to blur the shadow surface\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				// increase alpha and color in black (RGB=0)
				// with some stupid optimization for handling maximum values
				if (alpha < 255/4)
					*beg = (alpha*4) << 24;
				else
					*beg = 0xFF000000; // we hit the maximum
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}


surface recolor_image(surface surf, const std::map<Uint32, Uint32>& map_rgb, bool optimize){
	if(surf == NULL)
		return NULL;

	if(map_rgb.size()){
	     surface nsurf(clone_surface(surf));
	     if(nsurf == NULL) {
			std::cerr << "failed to make neutral surface\n";
			return NULL;
	     }

		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		std::map<Uint32, Uint32>::const_iterator map_rgb_end = map_rgb.end();

		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha){	// don't recolor invisible pixels.
				// palette use only RGB channels, so remove alpha
				Uint32 oldrgb = (*beg) & 0x00FFFFFF;
				std::map<Uint32, Uint32>::const_iterator i = map_rgb.find(oldrgb);
				if(i != map_rgb.end()){
					*beg = (alpha << 24) + i->second;
				}
			}
		++beg;
		}

		return optimize ? makesure_neutral_surface(nsurf) : nsurf;
	}
	return surf;
}

surface brighten_image(const surface &surf, fixed_t amount, bool optimize)
{
	if(surf == NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		if (amount < 0) amount = 0;
		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);

				r = (Uint8)std::min<unsigned>(unsigned(fxpmult(r, amount)),255);
				g = (Uint8)std::min<unsigned>(unsigned(fxpmult(g, amount)),255);
				b = (Uint8)std::min<unsigned>(unsigned(fxpmult(b, amount)),255);

				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface adjust_surface_alpha(const surface &surf, fixed_t amount, bool optimize)
{
	if(surf== NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		if (amount < 0) amount = 0;
		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);

				alpha = (Uint8)std::min<unsigned>(unsigned(fxpmult(alpha,amount)),255);
				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface adjust_surface_alpha_add(const surface &surf, int amount, bool optimize)
{
	if(surf== NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		while(beg != end) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);

				alpha = Uint8(std::max<int>(0,std::min<int>(255,int(alpha) + amount)));
				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface mask_surface(const surface &surf, const surface &mask, bool* empty_result)
{
	if(surf == NULL) {
		return NULL;
	}
	if(mask == NULL) {
		return surf;
	}
	VALIDATE(is_neutral_surface(mask), null_str);

	surface nsurf = clone_surface(surf);

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}
	if (nsurf->w !=  mask->w) {
		// we don't support efficiently different width.
		// (different height is not a real problem)
		// This function is used on all hexes and usually only for that
		// so better keep it simple and efficient for the normal case
		std::cerr << "Detected an image with bad dimensions :" << nsurf->w << "x" << nsurf->h << "\n";
		std::cerr << "It will not be masked, please use :"<< mask->w << "x" << mask->h << "\n";
		return nsurf;
	}

	bool empty = true;
	{
		surface_lock lock(nsurf);
		const_surface_lock mlock(mask);

		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;
		const Uint32* mbeg = mlock.pixels();
		const Uint32* mend = mbeg + mask->w*mask->h;

		while(beg != end && mbeg != mend) {
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);

				Uint8 malpha = (*mbeg) >> 24;
				if (alpha > malpha) {
					alpha = malpha;
				}
				if(alpha)
					empty = false;

				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
			++mbeg;
		}
	}
	if(empty_result)
		*empty_result = empty;

	return nsurf;
}

bool in_mask_surface(const surface &surf, const surface &mask)
{
	if(surf == NULL) {
		return false;
	}
	if(mask == NULL){
		return true;
	}

	if (surf->w != mask->w || surf->h != mask->h ) {
		// not same size, consider it doesn't fit
		return false;
	}
	VALIDATE(is_neutral_surface(mask), null_str);

	surface nsurf = clone_surface(surf);

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return false;
	}

	{
		surface_lock lock(nsurf);
		const_surface_lock mlock(mask);

		const Uint32* mbeg = mlock.pixels();
		const Uint32* mend = mbeg + mask->w*mask->h;
		Uint32* beg = lock.pixels();
		// no need for 'end', because both surfaces have same size

		while(mbeg != mend) {
			Uint8 malpha = (*mbeg) >> 24;
			if(malpha == 0) {
				Uint8 alpha = (*beg) >> 24;
				if (alpha)
					return false;
			}
			++mbeg;
			++beg;
		}
	}

	return true;
}

bool is_opaque(const surface& surf, bool approximate)
{
	if (surf == NULL) {
		return true;
	}

	bool opaque = true;
	Uint8 threshold = approximate? 246: 255; // this surface maybe scale, scale maybe decrease alpha
	{
		const_surface_lock mlock(surf);

		const Uint32* mbeg = mlock.pixels();
		const Uint32* mend = mbeg + surf->w * surf->h;

		while (mbeg != mend) {
			Uint8 alpha = (*mbeg) >> 24;

			if (alpha < threshold) {
				opaque = false;
				break;
			}


			++ mbeg;
		}
	}

	return opaque;
}

surface submerge_alpha(const surface &surf, int depth, float alpha_base, float alpha_delta,  bool optimize)
{
	if(surf== NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	{
		surface_lock lock(nsurf);

		Uint32* beg = lock.pixels();
		Uint32* limit = beg + (nsurf->h-depth) * nsurf->w ;
		Uint32* end = beg + nsurf->w * nsurf->h;
		beg = limit; // directlt jump to the bottom part

		while(beg != end){
			Uint8 alpha = (*beg) >> 24;

			if(alpha) {
				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);
				int d = (beg-limit)/nsurf->w;  // current depth in pixels
				float a = alpha_base - d * alpha_delta;
				fixed_t amount = ftofxp(a<0?0:a);
				alpha = (Uint8)std::min<unsigned>(unsigned(fxpmult(alpha,amount)),255);
				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}

			++beg;
		}

/*
		for(int y = submerge_height; y < nsurf->h; ++y) {
			Uint32* cur = beg + y * nsurf->w;
			Uint32* row_end = beg + (y+1) * nsurf->w;
			float d = y * 1.0 / depth;
			double a = 0.2;//std::max<double>(0, (1-d)*0.3);
			fixed_t amount = ftofxp(a);
			while(cur != row_end) {
				Uint8 alpha = (*cur) >> 24;

				if(alpha) {
					Uint8 r, g, b;
					r = (*cur) >> 16;
					g = (*cur) >> 8;
					b = (*cur);
					alpha = std::min<unsigned>(unsigned(fxpmult(alpha,amount)),255);
					*cur = (alpha << 24) + (r << 16) + (g << 8) + b;
				}

				++cur;
			}
		}*/

	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;

}

surface light_surface(const surface &surf, const surface &lightmap, bool optimize)
{
	if(surf == NULL) {
		return NULL;
	}
	if(lightmap == NULL) {
		return surf;
	}
	VALIDATE(is_neutral_surface(lightmap), null_str);

	surface nsurf = clone_surface(surf);

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}
	if (nsurf->w != lightmap->w) {
		// we don't support efficiently different width.
		// (different height is not a real problem)
		// This function is used on all hexes and usually only for that
		// so better keep it simple and efficient for the normal case
		std::cerr << "Detected an image with bad dimensions :" << nsurf->w << "x" << nsurf->h << "\n";
		std::cerr << "It will not be lighted, please use :"<< lightmap->w << "x" << lightmap->h << "\n";
		return nsurf;
	}
	{
		surface_lock lock(nsurf);
		const_surface_lock llock(lightmap);

		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w * nsurf->h;
		const Uint32* lbeg = llock.pixels();
		const Uint32* lend = lbeg + lightmap->w * lightmap->h;

		while(beg != end && lbeg != lend) {
			Uint8 alpha = (*beg) >> 24;
 			if(alpha) {
				Uint8 lr, lg, lb;

				lr = (*lbeg) >> 16;
				lg = (*lbeg) >> 8;
				lb = (*lbeg);

				Uint8 r, g, b;
				r = (*beg) >> 16;
				g = (*beg) >> 8;
				b = (*beg);

				r = std::max<int>(0,std::min<int>(255,int(r) + lr - 128));
				g = std::max<int>(0,std::min<int>(255,int(g) + lg - 128));
				b = std::max<int>(0,std::min<int>(255,int(b) + lb - 128));

				*beg = (alpha << 24) + (r << 16) + (g << 8) + b;
			}
			++beg;
			++lbeg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}


surface blur_surface(const surface &surf, int depth, bool optimize)
{
	if(surf == NULL) {
		return NULL;
	}

	surface res = clone_surface(surf);

	if(res == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	SDL_Rect rect = create_rect(0, 0, surf->w, surf->h);
	blur_surface(res, rect, depth);

	return optimize ? makesure_neutral_surface(res) : res;
}

void blur_surface(surface& surf, SDL_Rect rect, int depth)
{
	if(surf == NULL) {
		return;
	}

	const unsigned max_blur = 256;
	if(depth > max_blur) {
		depth = max_blur;
	}

	Uint32 queue[max_blur];
	const Uint32* end_queue = queue + max_blur;

	const Uint32 ff = 0xff;

	const unsigned pixel_offset = rect.y * surf->w + rect.x;

	surface_lock lock(surf);
	for(int y = 0; y < rect.h; ++y) {
		const Uint32* front = &queue[0];
		Uint32* back = &queue[0];
		Uint32 red = 0, green = 0, blue = 0, avg = 0;
		Uint32* p = lock.pixels() + pixel_offset + y * surf->w;
		for(int x = 0; x <= depth && x < rect.w; ++x, ++p) {
			red += ((*p) >> 16)&0xFF;
			green += ((*p) >> 8)&0xFF;
			blue += (*p)&0xFF;
			++avg;
			*back++ = *p;
			if(back == end_queue) {
				back = &queue[0];
			}
		}

		p = lock.pixels() + pixel_offset + y * surf->w;
		for(int x = 0; x < rect.w; ++x, ++p) {
			*p = 0xFF000000
					| (std::min(red/avg,ff) << 16)
					| (std::min(green/avg,ff) << 8)
					| std::min(blue/avg,ff);

			if(x >= depth) {
				red -= ((*front) >> 16)&0xFF;
				green -= ((*front) >> 8)&0xFF;
				blue -= *front&0xFF;
				--avg;
				++front;
				if(front == end_queue) {
					front = &queue[0];
				}
			}

			if(x + depth+1 < rect.w) {
				Uint32* q = p + depth+1;
				red += ((*q) >> 16)&0xFF;
				green += ((*q) >> 8)&0xFF;
				blue += (*q)&0xFF;
				++avg;
				*back++ = *q;
				if(back == end_queue) {
					back = &queue[0];
				}
			}
		}
	}

	for(int x = 0; x < rect.w; ++x) {
		const Uint32* front = &queue[0];
		Uint32* back = &queue[0];
		Uint32 red = 0, green = 0, blue = 0, avg = 0;
		Uint32* p = lock.pixels() + pixel_offset + x;
		for(int y = 0; y <= depth && y < rect.h; ++y, p += surf->w) {
			red += ((*p) >> 16)&0xFF;
			green += ((*p) >> 8)&0xFF;
			blue += *p&0xFF;
			++avg;
			*back++ = *p;
			if(back == end_queue) {
				back = &queue[0];
			}
		}

		p = lock.pixels() + pixel_offset + x;
		for(int y = 0; y < rect.h; ++y, p += surf->w) {
			*p = 0xFF000000
					| (std::min(red/avg,ff) << 16)
					| (std::min(green/avg,ff) << 8)
					| std::min(blue/avg,ff);

			if(y >= depth) {
				red -= ((*front) >> 16)&0xFF;
				green -= ((*front) >> 8)&0xFF;
				blue -= *front&0xFF;
				--avg;
				++front;
				if(front == end_queue) {
					front = &queue[0];
				}
			}

			if(y + depth+1 < rect.h) {
				Uint32* q = p + (depth+1)*surf->w;
				red += ((*q) >> 16)&0xFF;
				green += ((*q) >> 8)&0xFF;
				blue += (*q)&0xFF;
				++avg;
				*back++ = *q;
				if(back == end_queue) {
					back = &queue[0];
				}
			}
		}
	}
}

surface blur_alpha_surface(const surface &surf, int depth, bool optimize)
{
	if (surf == NULL) {
		return NULL;
	}

	surface res = clone_surface(surf);

	if(res == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	const int max_blur = 256;
	if(depth > max_blur) {
		depth = max_blur;
	}

	Uint32 queue[max_blur];
	const Uint32* end_queue = queue + max_blur;

	const Uint32 ff = 0xff;

	surface_lock lock(res);
	int x, y;
	for(y = 0; y < res->h; ++y) {
		const Uint32* front = &queue[0];
		Uint32* back = &queue[0];
		Uint32 alpha=0, red = 0, green = 0, blue = 0, avg = 0;
		Uint32* p = lock.pixels() + y*res->w;
		for(x = 0; x <= depth && x < res->w; ++x, ++p) {
			alpha += ((*p) >> 24)&0xFF;
			red += ((*p) >> 16)&0xFF;
			green += ((*p) >> 8)&0xFF;
			blue += (*p)&0xFF;
			++avg;
			*back++ = *p;
			if(back == end_queue) {
				back = &queue[0];
			}
		}

		p = lock.pixels() + y*res->w;
		for(x = 0; x < res->w; ++x, ++p) {
			*p = (std::min(alpha/avg,ff) << 24) | (std::min(red/avg,ff) << 16) | (std::min(green/avg,ff) << 8) | std::min(blue/avg,ff);
			if(x >= depth) {
				alpha -= ((*front) >> 24)&0xFF;
				red -= ((*front) >> 16)&0xFF;
				green -= ((*front) >> 8)&0xFF;
				blue -= *front&0xFF;
				--avg;
				++front;
				if(front == end_queue) {
					front = &queue[0];
				}
			}

			if(x + depth+1 < res->w) {
				Uint32* q = p + depth+1;
				alpha += ((*q) >> 24)&0xFF;
				red += ((*q) >> 16)&0xFF;
				green += ((*q) >> 8)&0xFF;
				blue += (*q)&0xFF;
				++avg;
				*back++ = *q;
				if(back == end_queue) {
					back = &queue[0];
				}
			}
		}
	}

	for(x = 0; x < res->w; ++x) {
		const Uint32* front = &queue[0];
		Uint32* back = &queue[0];
		Uint32 alpha=0, red = 0, green = 0, blue = 0, avg = 0;
		Uint32* p = lock.pixels() + x;
		for(y = 0; y <= depth && y < res->h; ++y, p += res->w) {
			alpha += ((*p) >> 24)&0xFF;
			red += ((*p) >> 16)&0xFF;
			green += ((*p) >> 8)&0xFF;
			blue += *p&0xFF;
			++avg;
			*back++ = *p;
			if(back == end_queue) {
				back = &queue[0];
			}
		}

		p = lock.pixels() + x;
		for(y = 0; y < res->h; ++y, p += res->w) {
			*p = (std::min(alpha/avg,ff) << 24) | (std::min(red/avg,ff) << 16) | (std::min(green/avg,ff) << 8) | std::min(blue/avg,ff);
			if(y >= depth) {
				alpha -= ((*front) >> 24)&0xFF;
				red -= ((*front) >> 16)&0xFF;
				green -= ((*front) >> 8)&0xFF;
				blue -= *front&0xFF;
				--avg;
				++front;
				if(front == end_queue) {
					front = &queue[0];
				}
			}

			if(y + depth+1 < res->h) {
				Uint32* q = p + (depth+1)*res->w;
				alpha += ((*q) >> 24)&0xFF;
				red += ((*q) >> 16)&0xFF;
				green += ((*q) >> 8)&0xFF;
				blue += (*q)&0xFF;
				++avg;
				*back++ = *q;
				if(back == end_queue) {
					back = &queue[0];
				}
			}
		}
	}

	return optimize ? makesure_neutral_surface(res) : res;
}

surface cut_surface(const surface &surf, SDL_Rect const &r)
{
	if (surf == NULL) {
		return NULL;
	}
	if (r.w <= 0 || r.h <= 0) {
		return NULL;
	}

	surface res = create_compatible_surface(surf, r.w, r.h);

	size_t sbpp = surf->format->BytesPerPixel;
	size_t spitch = surf->pitch;
	size_t rbpp = res->format->BytesPerPixel;
	size_t rpitch = res->pitch;

	// compute the areas to copy
	SDL_Rect src_rect = r;
	SDL_Rect dst_rect = { 0, 0, r.w, r.h };

	if (src_rect.x < 0) {
		if (src_rect.x + src_rect.w <= 0)
			return res;
		dst_rect.x -= src_rect.x;
		dst_rect.w += src_rect.x;
		src_rect.w += src_rect.x;
		src_rect.x = 0;
	}
	if (src_rect.y < 0) {
		if (src_rect.y + src_rect.h <= 0)
			return res;
		dst_rect.y -= src_rect.y;
		dst_rect.h += src_rect.y;
		src_rect.h += src_rect.y;
		src_rect.y = 0;
	}

	if(src_rect.x >= surf->w || src_rect.y >= surf->h)
		return res;

	const_surface_lock slock(surf);
	surface_lock rlock(res);

	const Uint8* src = reinterpret_cast<const Uint8 *>(slock.pixels());
	Uint8* dest = reinterpret_cast<Uint8 *>(rlock.pixels());

	for(int y = 0; y < src_rect.h && (src_rect.y + y) < surf->h; ++y) {
		const Uint8* line_src  = src  + (src_rect.y + y) * spitch + src_rect.x * sbpp;
		Uint8* line_dest = dest + (dst_rect.y + y) * rpitch + dst_rect.x * rbpp;
		size_t size = src_rect.w + src_rect.x <= surf->w ? src_rect.w : surf->w - src_rect.x;

		assert(rpitch >= src_rect.w * rbpp);
		memcpy(line_dest, line_src, size * rbpp);
	}

	return res;
}

surface blend_surface(const surface &surf, double amount, Uint32 color, bool optimize)
{
	if (surf== NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* beg = lock.pixels();
		Uint32* end = beg + nsurf->w*surf->h;

		Uint8 red, green, blue, alpha;
		SDL_GetRGBA(color,nsurf->format,&red,&green,&blue,&alpha);

		red   = Uint8(red   * amount);
		green = Uint8(green * amount);
		blue  = Uint8(blue  * amount);

		amount = 1.0 - amount;

		while(beg != end) {
			Uint8 r, g, b, a;
			a = (*beg) >> 24;
			r = (*beg) >> 16;
			g = (*beg) >> 8;
			b = (*beg);

			r = Uint8(r * amount) + red;
			g = Uint8(g * amount) + green;
			b = Uint8(b * amount) + blue;

			*beg = (a << 24) | (r << 16) | (g << 8) | b;

			++beg;
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface flip_surface(const surface &surf, bool optimize)
{
	if (surf == NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if (nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* const pixels = lock.pixels();

		for(int y = 0; y != nsurf->h; ++y) {
			for(int x = 0; x != nsurf->w/2; ++x) {
				const int index1 = y*nsurf->w + x;
				const int index2 = (y+1)*nsurf->w - x - 1;
				std::swap(pixels[index1],pixels[index2]);
			}
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface flop_surface(const surface &surf, bool optimize)
{
	if (surf == NULL) {
		return NULL;
	}

	surface nsurf(clone_surface(surf));

	if(nsurf == NULL) {
		std::cerr << "could not make neutral surface...\n";
		return NULL;
	}

	{
		surface_lock lock(nsurf);
		Uint32* const pixels = lock.pixels();

		for(int x = 0; x != nsurf->w; ++x) {
			for(int y = 0; y != nsurf->h/2; ++y) {
				const int index1 = y*nsurf->w + x;
				const int index2 = (nsurf->h-y-1)*surf->w + x;
				std::swap(pixels[index1],pixels[index2]);
			}
		}
	}

	return optimize ? makesure_neutral_surface(nsurf) : nsurf;
}

surface rotate_surface(const surface& surf, double angle)
{
	if (!surf) {
		return NULL;
	}

	int flip = 0;
	int dstwidth, dstheight;
    double cangle, sangle;
	SDLgfx_rotozoomSurfaceSizeTrig(surf->w, surf->h, -angle, &dstwidth, &dstheight, &cangle, &sangle);
	surface surface_rotated = SDLgfx_rotateSurface(surf, -angle, dstwidth/2, dstheight/2, 1, flip & SDL_FLIP_HORIZONTAL, flip & SDL_FLIP_VERTICAL, dstwidth, dstheight, cangle, sangle);
	return surface_rotated;
}

surface rotate_surface2(const surface& surf, int srcx, int srcy, int degree, int offsetx, int offsety, int& dstx, int& dsty)
{
	if (!surf) {
		return NULL;
	}

	VALIDATE(degree >= 0, null_str);
	degree %= 360;

	int flip = 0;
	const double angle = degree;
	int dstwidth, dstheight;
    double cangle, sangle;
	SDLgfx_rotozoomSurfaceSizeTrig(surf->w, surf->h, -angle, &dstwidth, &dstheight, &cangle, &sangle);
	surface surface_rotated = SDLgfx_rotateSurface(surf, -angle, dstwidth/2, dstheight/2, 1, flip & SDL_FLIP_HORIZONTAL, flip & SDL_FLIP_VERTICAL, dstwidth, dstheight, cangle, sangle);

	int dstwidth2, dstheight2;
	SDLgfx_rotozoomSurfaceSizeTrig2(offsetx, offsety, cangle, sangle, &dstwidth2, &dstheight2);

	if (degree < 90) {
		dstx = srcx - dstwidth2;
		dsty = srcy - dstheight + dstheight2;

	} else if (degree < 180) {
		dstx = srcx - dstwidth2;
		dsty = srcy - dstheight2;

	} else if (degree < 270) {
		dstx = srcx - dstwidth + dstwidth2;
		dsty = srcy - dstheight2;

	} else {
		dstx = srcx - dstwidth + dstwidth2;
		dsty = srcy - dstheight + dstheight2;
	}
	return surface_rotated;
}

surface create_compatible_surface(const surface &surf, int width, int height)
{
	if (surf == NULL) {
		return NULL;
	}

	if(width == -1)
		width = surf->w;

	if(height == -1)
		height = surf->h;

	surface s = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, surf->format->BitsPerPixel,
		surf->format->Rmask, surf->format->Gmask, surf->format->Bmask, surf->format->Amask);
	if (surf->format->palette) {
		SDL_SetPaletteColors(s->format->palette, surf->format->palette->colors, 0, surf->format->palette->ncolors);
	}
	return s;
}

void render_surface(SDL_Renderer* renderer, const surface& surf, const SDL_Rect* srcrect, const SDL_Rect* dstrect)
{
	if (surf) {
		texture src = SDL_CreateTextureFromSurface(renderer, surf);
		SDL_RenderCopy(renderer, src.get(), srcrect, dstrect);
	}
}

void texture_from_texture(const texture& src, texture& dst, const SDL_Rect* srcrect, const int dstwidth, const int dstheight)
{
	SDL_Renderer* renderer = get_renderer();
	int src_width, src_height;
	uint32_t format;
	bool create_locally = false;
	SDL_QueryTexture(src.get(), &format, NULL, &src_width, &src_height);

	SDL_Rect real_srcrect = ::create_rect(0, 0, src_width, src_height);
	if (srcrect) {
		real_srcrect = *srcrect;
	}

	if (dst.get() == NULL) {
		const int real_dstwidth = dstwidth? dstwidth: real_srcrect.w;
		const int real_dstheight = dstheight? dstheight: real_srcrect.h;
		dst = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, real_dstwidth, real_dstheight);
		create_locally = true;
	}

	// below will change target. require save/recover clip setting of preview target.
	texture_clip_rect_setter clip(NULL);
	{
		trender_target_lock lock(renderer, dst);
		ttexture_blend_none_lock lock2(src);
		if (create_locally) {
			SDL_RenderClear(renderer);
		}
		int dst_width, dst_height;
		SDL_QueryTexture(dst.get(), NULL, NULL, &dst_width, &dst_height);
		SDL_Rect dst_rect = ::create_rect(0, 0, dst_width, dst_height);
		// if x/y < 0, dst, requrie offset.
		if (real_srcrect.x < 0) {
			dst_rect.x = -1 * real_srcrect.x;
			dst_rect.w += real_srcrect.x;
		}
		if (real_srcrect.y < 0) {
			dst_rect.y = -1 * real_srcrect.y;
			dst_rect.h += real_srcrect.y;
		}

		// if w/h < dst.w/dst.h, require clip.
		const SDL_Rect src_texture_rect = ::create_rect(0, 0, src_width, src_height);
		real_srcrect = intersect_rects(src_texture_rect, real_srcrect);

		if (real_srcrect.w < dst_rect.w) {
			dst_rect.w = real_srcrect.w;
		}
		if (real_srcrect.h < dst_rect.h) {
			dst_rect.h = real_srcrect.h;
		}
		SDL_RenderCopy(renderer, src.get(), &real_srcrect, &dst_rect);
	}
}

texture clone_texture(const texture& src, const uint8_t r, const uint8_t g, const uint8_t b)
{
	if (src.get() == NULL) {
		return NULL;
	}

	SDL_Renderer* renderer = get_renderer();
	int src_width, src_height;
	uint32_t format;
	SDL_QueryTexture(src.get(), &format, NULL, &src_width, &src_height);

	texture res = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, src_width, src_height);
	texture_clip_rect_setter clip(NULL);
	{
		trender_target_lock lock(renderer, res);
		ttexture_blend_none_lock lock2(src);
		SDL_RenderClear(renderer);

		ttexture_color_mod_lock lock3(src, r, g, b);
		SDL_RenderCopy(renderer, src.get(), NULL, NULL);
	}

	SDL_BlendMode blendmode;
	SDL_GetTextureBlendMode(src.get(), &blendmode);
	SDL_SetTextureBlendMode(res.get(), blendmode);

	return res;
}

void brighten_renderer(SDL_Renderer* renderer, const uint8_t r, const uint8_t g, const uint8_t b, const SDL_Rect* dstrect)
{
	texture texture = get_white_texture();

	ttexture_color_mod_lock lock3(texture, r, g, b);
	SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_ADD);
	SDL_RenderCopy(renderer, texture.get(), NULL, dstrect);
}

void brighten_texture(const texture& tex, const uint8_t r, const uint8_t g, const uint8_t b)
{
	SDL_Renderer* renderer = get_renderer();
	{
		texture_clip_rect_setter clip_setter(NULL);
		trender_target_lock lock(renderer, tex);
		brighten_renderer(renderer, r, g, b, NULL);
	}
}

void grayscale_renderer(SDL_Renderer* renderer)
{
	SDL_Texture* target = SDL_GetRenderTarget(renderer);
	VALIDATE(target, null_str);

	int tex_width, tex_height;
	uint32_t format;
	SDL_QueryTexture(target, &format, NULL, &tex_width, &tex_height);
	surface surf = create_neutral_surface(tex_width, tex_height);
	{
		surface_lock dst_lock(surf);
		SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, 4 * tex_width);
	}
	surf = greyscale_image(surf, false);
	SDL_UpdateTexture(target, NULL, surf->pixels, 4 * tex_width);
}

texture target_texture_from_surface(const surface& surf)
{
	if (surf.get() == NULL) {
		return NULL;
	}
	SDL_Renderer* renderer = get_renderer();
	SDL_Texture* tex = SDL_CreateTexture(renderer, surf->format->format, SDL_TEXTUREACCESS_TARGET, surf->w, surf->h);
	{
		// some surf maybe been blit before, must RLE decode.
		const_surface_lock dst_lock(surf);
		SDL_UpdateTexture(tex, NULL, surf->pixels, surf->pitch);
	}

	SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
	return tex;
}

void imwrite(const surface& surf, const std::string& path)
{
	VALIDATE(surf.get(), null_str);
	VALIDATE(!path.empty() && path[0] != '/', null_str);

	if (surf->format->BytesPerPixel == 3) {
		// on little-end, if want to save 3 component, compare to argb, require swip r,b.
		// reference to surface(const cv::Mat& mat)
		tsurface_2_mat_lock lock(surf);
		cv::cvtColor(lock.mat, lock.mat, cv::COLOR_BGR2RGB);
	}
	IMG_SavePNG(surf, (game_config::preferences_dir + "/" + path).c_str());
}

void imwrite(const texture& tex, const std::string& path)
{
	VALIDATE(tex.get(), null_str);
	VALIDATE(!path.empty() && path[0] != '/', null_str);

	int width, height;
	uint32_t format;
	SDL_QueryTexture(tex.get(), &format, NULL, &width, &height);

	surface dst = create_neutral_surface(width, height);
	{
		trender_target_lock target_lock(get_renderer(), tex);
		texture_clip_rect_setter clip_rect(NULL);
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), NULL, format, dst->pixels, 4 * width);
	}
	IMG_SavePNG(dst, (game_config::preferences_dir + "/" + path).c_str());
}

surface save_texture_to_surface(const texture& tex)
{
	int width, height;
	uint32_t format;
	SDL_QueryTexture(tex.get(), &format, NULL, &width, &height);

	surface dst = create_neutral_surface(width, height);
	{
		trender_target_lock target_lock(get_renderer(), tex);
		texture_clip_rect_setter clip_rect(NULL);
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), NULL, format, dst->pixels, 4 * width);
	}
	return dst;
}

void fill_rect_alpha(SDL_Rect &rect, Uint32 color, Uint8 alpha, surface &target)
{
	if(alpha == SDL_ALPHA_OPAQUE) {
		sdl_fill_rect(target,&rect,color);
		return;
	} else if(alpha == SDL_ALPHA_TRANSPARENT) {
		return;
	}

	surface tmp(create_compatible_surface(target,rect.w,rect.h));
	if(tmp == NULL) {
		return;
	}

	SDL_Rect r = {0,0,rect.w,rect.h};
	sdl_fill_rect(tmp, &r, color);
	SDL_SetSurfaceBlendMode(tmp, SDL_BLENDMODE_BLEND);
	sdl_blit(tmp, NULL, target, &rect);
}

surface get_surface_portion(const texture& src, SDL_Rect &area)
{
	if (src == NULL) {
		return NULL;
	}

	if (area.w <= 0 || area.h <= 0) {
		return NULL;
	}

	int src_w, src_h;
	Uint32 src_format;
	SDL_QueryTexture(src.get(), &src_format, NULL, &src_w, &src_h);
	// Check if there is something in the portion
	if (area.x >= src_w || area.y >= src_h || area.x + area.w < 0 || area.y + area.h < 0) {
		return NULL;
	}

	if (area.x + area.w > src_w) {
		area.w = src_w - area.x;
	}
	if (area.y + area.h > src_h) {
		area.h = src_h - area.y;
	}

	SDL_PixelFormat format2 = get_neutral_pixel_format();
	VALIDATE(src_format == get_neutral_pixel_format().format, null_str);

	// use same format as the source (almost always the screen)
	surface dst = create_neutral_surface(area.w, area.h);
	if (dst == NULL) {
		std::cerr << "Could not create a new surface in get_surface_portion()\n";
		return NULL;
	}

	{
		surface_lock dst_lock(dst);
		SDL_RenderReadPixels(get_renderer(), &area, src_format, dst->pixels, 4 * area.w);
	}
	return dst;
}

surface get_surface_portion2(const surface &src, SDL_Rect &area)
{
	if (src == NULL) {
		return NULL;
	}

	if (area.w <= 0 || area.h <= 0) {
		return NULL;
	}

	// Check if there is something in the portion
	if (area.x >= src->w || area.y >= src->h || area.x + area.w < 0 || area.y + area.h < 0) {
		return NULL;
	}

	if (area.x + area.w > src->w) {
		area.w = src->w - area.x;
	}
	if (area.y + area.h > src->h) {
		area.h = src->h - area.y;
	}

	VALIDATE(is_neutral_surface(src), null_str);

	// use same format as the source (almost always the screen)
	surface dst = create_compatible_surface(src, area.w, area.h);
	if (dst == NULL) {
		std::cerr << "Could not create a new surface in get_surface_portion()\n";
		return NULL;
	}

	sdl_blit(src, &area, dst, NULL);
	return dst;
}

namespace {

struct not_alpha
{
	not_alpha() {}

	// we assume neutral format
	bool operator()(Uint32 pixel) const {
		Uint8 alpha = pixel >> 24;
		return alpha != 0x00;
	}
};

}

SDL_Rect get_non_transparent_portion(const surface &surf)
{
	SDL_Rect res = {0,0,0,0};
	surface nsurf(clone_surface(surf));
	if(nsurf == NULL) {
		std::cerr << "failed to make neutral surface\n";
		return res;
	}

	const not_alpha calc;

	surface_lock lock(nsurf);
	const Uint32* const pixels = lock.pixels();

	int n;
	for(n = 0; n != nsurf->h; ++n) {
		const Uint32* const start_row = pixels + n*nsurf->w;
		const Uint32* const end_row = start_row + nsurf->w;

		if(std::find_if(start_row,end_row,calc) != end_row)
			break;
	}

	res.y = n;

	for(n = 0; n != nsurf->h-res.y; ++n) {
		const Uint32* const start_row = pixels + (nsurf->h-n-1)*surf->w;
		const Uint32* const end_row = start_row + nsurf->w;

		if(std::find_if(start_row,end_row,calc) != end_row)
			break;
	}

	// The height is the height of the surface,
	// minus the distance from the top and
	// the distance from the bottom.
	res.h = nsurf->h - res.y - n;

	for(n = 0; n != nsurf->w; ++n) {
		int y;
		for(y = 0; y != nsurf->h; ++y) {
			const Uint32 pixel = pixels[y*nsurf->w + n];
			if(calc(pixel))
				break;
		}

		if(y != nsurf->h)
			break;
	}

	res.x = n;

	for(n = 0; n != nsurf->w-res.x; ++n) {
		int y;
		for(y = 0; y != nsurf->h; ++y) {
			const Uint32 pixel = pixels[y*nsurf->w + surf->w - n - 1];
			if(calc(pixel))
				break;
		}

		if(y != nsurf->h)
			break;
	}

	res.w = nsurf->w - res.x - n;

	return res;
}

bool operator==(const SDL_Rect& a, const SDL_Rect& b)
{
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool operator!=(const SDL_Rect& a, const SDL_Rect& b)
{
	return !operator==(a,b);
}

/*
// NDK's llvm doesn't support below std::set format.
bool operator<(const SDL_Rect& a, const SDL_Rect& b)
{
	if (a.x != b.x) {
		return a.x < b.x;
	}
	if (a.y != b.y) {
		return a.y < b.y;
	}
	if (a.w != b.w) {
		return a.w < b.w;
	}
	return a.h < b.h;
}
*/

bool operator==(const SDL_Color& a, const SDL_Color& b) {
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

bool operator!=(const SDL_Color& a, const SDL_Color& b) {
	return !operator==(a,b);
}

SDL_Color inverse(const SDL_Color& color) {
	SDL_Color inverse;
	inverse.r = 255 - color.r;
	inverse.g = 255 - color.g;
	inverse.b = 255 - color.b;
	inverse.a = 0;
	return inverse;
}

void draw_rectangle(int x, int y, int w, int h, Uint32 color,surface target)
{

	SDL_Rect top = create_rect(x, y, w, 1);
	SDL_Rect bot = create_rect(x, y + h - 1, w, 1);
	SDL_Rect left = create_rect(x, y, 1, h);
	SDL_Rect right = create_rect(x + w - 1, y, 1, h);

	sdl_fill_rect(target,&top,color);
	sdl_fill_rect(target,&bot,color);
	sdl_fill_rect(target,&left,color);
	sdl_fill_rect(target,&right,color);
}

void draw_centered_on_background(surface surf, const SDL_Rect& rect, const SDL_Color& color, surface target)
{
	clip_rect_setter clip_setter(target, &rect);

	Uint32 col = SDL_MapRGBA(target->format, color.r, color.g, color.b, color.a);
	//TODO: only draw background outside the image
	SDL_Rect r = rect;
	sdl_fill_rect(target, &r, col);

	if (surf != NULL) {
		r.x = rect.x + (rect.w-surf->w)/2;
		r.y = rect.y + (rect.h-surf->h)/2;
		sdl_blit(surf, NULL, target, &r);
	}
}

void blit_integer_surface(int integer, surface& to, int x, int y)
{
	const int digit_width = 8;
	const int digit_height = 12;
	if (to->w < 8) {
		return;
	}
	if (to->h < digit_height) {
		return;
	}
	if (integer < 0) {
		integer *= -1;
	}

	std::stringstream ss;
	SDL_Rect dst_clip = create_rect(x, y, 0, 0);

	int digit, max = 10;
	while (max <= integer) {
		max *= 10;
	}

	do {
		max /= 10;
		if (max) {
			digit = integer / max;
			integer %= max;
		} else {
			digit = integer;
			integer = 0;
		}

		ss.str("");
		ss << "misc/digit.png~CROP(" << (8 * digit) << ", 0, 8, 12)";
		sdl_blit(image::get_image(ss.str()), NULL, to, &dst_clip);
		dst_clip.x += digit_width;
		if (dst_clip.x > to->w) {
			break;
		}
	} while (max > 1);
}

surface generate_integer_surface(int integer)
{
	const int digit_width = 8;
	const int digit_height = 12;

	if (integer < 0) {
		integer *= -1;
	}

	std::stringstream ss;
	SDL_Rect dst_clip = create_rect(0, 0, 0, 0);

	int digit, digits = 1, max = 10;
	while (max <= integer) {
		max *= 10;
		digits ++;
	}

	surface result = create_neutral_surface(digits * digit_width, digit_height);

	do {
		max /= 10;
		if (max) {
			digit = integer / max;
			integer %= max;
		} else {
			digit = integer;
			integer = 0;
		}

		VALIDATE(dst_clip.x < result->w, null_str);

		ss.str("");
		ss << "misc/digit.png~CROP(" << (8 * digit) << ", 0, 8, 12)";
		sdl_blit(image::get_image(ss.str()), nullptr, result, &dst_clip);
		dst_clip.x += digit_width;

	} while (max > 1);

	VALIDATE(dst_clip.x == result->w, null_str);

	return result;
}

surface generate_pip_surface(surface& bg, surface& fg)
{
	if (!bg) {
		return surface();
	}
	surface result = clone_surface(bg);
	if (fg) {
		SDL_Rect dst_clip = create_rect(0, 0, 0, 0);
		if (result->w > fg->w) {
			dst_clip.x = (result->w - fg->w) / 2;
		}
		if (result->h > fg->h) {
			dst_clip.y = (result->h - fg->h) / 2;
		}
		sdl_blit(fg, NULL, result, &dst_clip);
	}

	return result;
}

surface generate_pip_surface(int width, int height, const std::string& bg, const std::string& fg)
{
	surface bg_surf = image::get_image(bg);
	surface fg_surf = image::get_image(fg);
	surface result = generate_pip_surface(bg_surf, fg_surf);

	if (width && height) {
		result = scale_surface(result, width, height);
	}
	return result;
}

surface generate_surface(int width, int height, const std::string& img, int integer, bool greyscale)
{
	surface surf = image::get_image(img);
	if (!surf) {
		return surf;
	}

	if (greyscale) {
		surf = greyscale_image(surf);
	}
	surf = scale_surface(surf, width, height);

	if (integer > 0) {
		blit_integer_surface(integer, surf, 0, 0);
	}
	return surf;
}

//
// circle, arc function
//
// extract pixels from surf to circle_pixels_
tarc_pixel* circle_calculate_pixels(surface& surf, int* valid_pixels)
{
	VALIDATE(surf->w == surf->h, null_str);
	const int diameter = surf->w;

	surface_lock lock(surf);
	Uint32* beg = lock.pixels();
	Uint32* end = beg + diameter * diameter;

	int circle_valid_pixels = 0;
	while (beg != end) {
		Uint8 alpha = (*beg) >> 24;
		if (alpha) {
			circle_valid_pixels ++;
		}
		++ beg;
	}

	tarc_pixel* circle_pixels = NULL;
	if (circle_valid_pixels) {
		circle_pixels = (tarc_pixel*)malloc(circle_valid_pixels * sizeof(tarc_pixel));
		beg = lock.pixels();
		int at = 0;
		const int originx = diameter / 2;
		const int originy = originx;
		int diffx, diffy;
		for (int row = 0; row < diameter; row ++) {
			for (int col = 0; col < diameter; col ++) {
				if (beg[row * diameter + col] >> 24) {
					tarc_pixel* pixel = circle_pixels + at;
					pixel->x = col;
					pixel->y = row;

					if (col >= originx && row < originy) {
						diffx = col - originx;
						diffy = originy - row;
						pixel->degree = atan(1.0 * diffx / diffy) * 180 / M_PI;

					} else if (col >= originx && row >= originy) {
						diffx = col - originx;
						diffy = row - originy;
						pixel->degree = atan(1.0 * diffy / diffx) * 180 / M_PI + 90;

					} else if (col < originx && row >= originy) {
						diffx = originx - col;
						diffy = row - originy;
						pixel->degree = atan(1.0 * diffx / diffy) * 180 / M_PI + 180;

					} else if (col < originx && row < originy) {
						diffx = originx - col;
						diffy = originy - row;
						pixel->degree = atan(1.0 * diffy / diffx) * 180 / M_PI + 270;
					}

					at ++;
				}
			}
		}
		VALIDATE(at == circle_valid_pixels, null_str);
	}
	if (valid_pixels) {
		*valid_pixels = circle_valid_pixels;
	}
	return circle_pixels;
}

// erase_col: ARGB
void circle_draw_arc(surface& surf, tarc_pixel* circle_pixels, const int circle_valid_pixels, int start, int stop, Uint32 erase_col)
{
	VALIDATE(start >= 0 && start < 360 && stop >= 0 && stop < 360, null_str);
	const int diameter = surf->w;
	surface_lock lock(surf);

	Uint32* beg = lock.pixels();
	for (int at = 0; at < circle_valid_pixels; at ++) {
		const tarc_pixel& coor = circle_pixels[at];
		if (start <= stop) {
			if (!(coor.degree >= start && coor.degree <= stop)) {
				beg[coor.y * diameter + coor.x] = erase_col;
			}
		} else {
			if (coor.degree < start && coor.degree > stop) {
				beg[coor.y * diameter + coor.x] = erase_col;
			}
		}
	}
}

std::ostream& operator<<(std::ostream& s, const SDL_Rect& rect)
{
	s << rect.x << ',' << rect.y << " x "  << rect.w << ',' << rect.h;
	return s;
}
