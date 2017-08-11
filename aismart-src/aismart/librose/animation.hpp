/* $Id: unit_animation.hpp 47611 2010-11-21 13:56:47Z mordante $ */
/*
   Copyright (C) 2006 - 2010 by Jeremy Rosen <jeremy.rosen@enst-bretagne.fr>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
   */
#ifndef LIBROSE_ANIMATION_H_INCLUDED
#define LIBROSE_ANIMATION_H_INCLUDED

#include "animated.hpp"
#include "config.hpp"
#include "unit_frame.hpp"
#include "video.hpp"

#include <list>
#include <boost/function.hpp>

enum tanim_type {anim_map, anim_canvas, anim_window};

class base_controller;
class base_unit;

class animation
{
public:
	const unit_frame& get_last_frame() const{ return unit_anim_.get_last_frame() ; };
	void add_frame(int duration, const unit_frame& value, bool force_change =false)
	{ 
		unit_anim_.add_frame(duration,value,force_change) ; 
	};

	bool need_update() const;
	bool need_minimal_update() const;
	bool animation_finished() const;
	bool animation_finished_potential() const;
	void update_last_draw_time();
	int get_begin_time() const;
	int get_end_time() const;
	int time_to_tick(int animation_time) const { return unit_anim_.time_to_tick(animation_time); };
	int get_animation_time() const{ return unit_anim_.get_animation_time() ; };
	int get_animation_time_potential() const{ return unit_anim_.get_animation_time_potential() ; };
	void start_animation(int start_time
			, const map_location &src = map_location::null_location
			, const map_location &dst = map_location::null_location
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0
			, const bool accelerate = true);
	void update_parameters(const map_location &src, const map_location &dst);
	void pause_animation();
	void restart_animation();
	int get_current_frame_begin_time() const{ return unit_anim_.get_current_frame_begin_time() ; };
	void redraw(frame_parameters& value);
	void clear_haloes();
	virtual bool invalidate(const int fb_width, const int fb_height, frame_parameters& value);
	bool invalidate(const int fb_width, const int fb_height);
	void replace_image_name(const std::string& part, const std::string& src, const std::string& dst);
	void replace_image_mod(const std::string& part, const std::string& src, const std::string& dst);
	void replace_progressive(const std::string& part, const std::string& name, const std::string& src, const std::string& dst);
	void replace_static_text(const std::string& part, const std::string& src, const std::string& dst);
	void replace_int(const std::string& part, const std::string& name, int src, int dst);
	int layer() const { return layer_; }
	bool cycles() const { return cycles_; }
	bool started() const { return started_; }

	tanim_type type() const { return type_; }
	void set_type(tanim_type type) { type_ = type; }

	virtual void redraw(texture& screen, const SDL_Rect& rect, bool snap_bg);
	virtual void undraw(texture& screen) {}

	void set_callback_finish(boost::function<void (animation*)> callback) { callback_finish_ = callback; }
	void require_release();

	bool area_mode() const { return area_mode_; }

	// explicit animation(const config &cfg, const std::string &frame_string = "");
	explicit animation(const config &cfg);

	// reserved to class unit, for the special case of redrawing the unit base frame
	const frame_parameters get_current_params(const frame_parameters & default_val = frame_parameters()) const
	{ 
		return unit_anim_.parameters(default_val); 
	};

protected:
	explicit animation(int start_time
				, const unit_frame &frame
				, const std::string& event
				, const int variation
				, const frame_parsed_parameters& builder = frame_parsed_parameters(config()));

	class particular:public animated<unit_frame>
	{
		public:
			explicit particular(int start_time=0, const frame_parsed_parameters &builder = frame_parsed_parameters(config())) :
				animated<unit_frame>(start_time),
				accelerate(true),
				cycles(false),
				parameters_(builder),
				halo_id_(0),
				last_frame_begin_time_(0)
				{};
			explicit particular(const config& cfg, const std::string& frame_string = "frame");

			virtual ~particular();
			bool need_update() const;
			bool need_minimal_update() const;
			void override(int start_time
					, int duration
					, const std::string& highlight = ""
					, const std::string& blend_ratio =""
					, Uint32 blend_color = 0
					, const std::string& offset = ""
					, const std::string& layer = ""
					, const std::string& modifiers = "");
			void redraw( const frame_parameters& value,const map_location &src, const map_location &dst);
			std::set<map_location> get_overlaped_hex(const frame_parameters& value, const map_location &src, const map_location &dst);
			std::vector<SDL_Rect> get_overlaped_rect(const frame_parameters& value, const map_location &src, const map_location &dst);
			void start_animation(int start_time, bool cycles=false);
			const frame_parameters parameters(const frame_parameters & default_val) const { return get_current_frame().merge_parameters(get_current_frame_time(),parameters_.parameters(get_animation_time()-get_begin_time()),default_val); };
			void clear_halo();
			void replace_image_name(const std::string& src, const std::string& dst);
			void replace_image_mod(const std::string& src, const std::string& dst);
			void replace_progressive(const std::string& name, const std::string& src, const std::string& dst);
			void replace_static_text(const std::string& src, const std::string& dst);
			void replace_int(const std::string& name, int src, int dst);

			bool accelerate;
			bool cycles;
		private:

			//animation params that can be locally overridden by frames
			frame_parsed_parameters parameters_;
			int halo_id_;
			int last_frame_begin_time_;

	};

protected:
	boost::function<void (animation*)> callback_finish_;

	tanim_type type_;
	particular unit_anim_;
	std::map<std::string, particular> sub_anims_;
	/* these are drawing parameters, but for efficiancy reason they are in the anim and not in the particle */
	map_location src_;
	map_location dst_;

	// optimization
	bool invalidated_;
	bool play_offscreen_;
	bool area_mode_;
	int layer_;
	bool cycles_;
	bool started_;
	std::set<map_location> overlaped_hex_;
};

class base_animator
{
public:
	base_animator()
		: animated_units_()
		, start_time_(INT_MIN)
	{}


	void add_animation2(base_unit* animated_unit
			, const animation * anim
			, const map_location& src = map_location::null_location
			, bool with_bars = false
			, bool cycles = false
			, const std::string& text = ""
			, const Uint32 text_color = 0);
	
	void start_animations();
	void pause_animation();
	void restart_animation();
	void clear() 
	{
		start_time_ = INT_MIN; 
		animated_units_.clear();
	}
	void set_all_standing();

	bool would_end() const;
	int get_animation_time() const;
	int get_animation_time_potential() const;
	int get_end_time() const;
	void wait_for_end(base_controller& controller) const;
	void wait_until(base_controller& controller, int animation_time) const;

	struct anim_elem 
	{
		anim_elem()
			: my_unit(0)
			, anim(0)
			, text()
			, text_color(0)
			, src()
			, with_bars(false)
			, cycles(false)
		{}

		base_unit* my_unit;
		const animation* anim;
		std::string text;
		Uint32 text_color;
		map_location src;
		bool with_bars;
		bool cycles;
	};
	std::vector<anim_elem>& animated_units() { return animated_units_; }
	const std::vector<anim_elem>& animated_units() const { return animated_units_; }

protected:
	std::vector<anim_elem> animated_units_;
	int start_time_;
};

class tdrawing_buffer
{
public:
	tdrawing_buffer(CVideo& video)
		: video_(video)
	{}
	~tdrawing_buffer();

	CVideo& video() { return video_; }

	enum tdrawing_layer{
		LAYER_TERRAIN_BG,          /**<
		                            * Layer for the terrain drawn behind the
		                            * unit.
		                            */
		LAYER_GRID_TOP,            /**< Top half part of grid image */
		LAYER_MOUSEOVER_OVERLAY,   /**< Mouseover overlay used by editor*/
		LAYER_FOOTSTEPS,           /**< Footsteps showing path from unit to mouse */
		LAYER_MOUSEOVER_TOP,       /**< Top half of image following the mouse */
		LAYER_UNIT_FIRST,          /**< Reserve layers to be selected for WML. */
		LAYER_UNIT_BG = LAYER_UNIT_FIRST+10,             /**< Used for the ellipse behind the unit. */
		LAYER_UNIT_DEFAULT=LAYER_UNIT_FIRST+40,/**<default layer for drawing units */
		LAYER_TERRAIN_FG = LAYER_UNIT_FIRST+50, /**<
		                            * Layer for the terrain drawn in front of
		                            * the unit.
		                            */
		LAYER_GRID_BOTTOM,         /**<
		                            * Used for the bottom half part of grid image.
		                            * Should be under moving units, to avoid masking south move.
		                            */
		LAYER_UNIT_MOVE_DEFAULT=LAYER_UNIT_FIRST+60/**<default layer for drawing moving units */,
		LAYER_UNIT_FG =  LAYER_UNIT_FIRST+80, /**<
		                            * Used for the ellipse in front of the
		                            * unit.
		                            */
		LAYER_UNIT_MISSILE_DEFAULT = LAYER_UNIT_FIRST+90, /**< default layer for missile frames*/
		LAYER_UNIT_LAST=LAYER_UNIT_FIRST+100,
		LAYER_UNIT_BAR=LAYER_UNIT_LAST+10,            /**<
		                            * Unit bars and overlays are drawn on this
		                            * layer (for testing here).
		                            */
		LAYER_REACHMAP,            /**< "black stripes" on unreachable hexes. */
		LAYER_MOUSEOVER_BOTTOM,    /**< Bottom half of image following the mouse */
		LAYER_FOG_SHROUD,          /**< Fog and shroud. */
		LAYER_ARROWS,              /**< Arrows from the arrows framework. Used for planned moves display. */
		LAYER_SELECTED_HEX,        /**< Image on the selected unit */
		LAYER_ATTACK_INDICATOR,    /**< Layer which holds the attack indicator. */

		LAYER_MOVE_INFO,           /**< Movement info (defense%, ect...). */
		LAYER_LINGER_OVERLAY,      /**< The overlay used for the linger mode. */
		LAYER_BORDER,              /**< The border of the map. */

		LAYER_HALO_DEFAULT,        /**< Halo. */

		LAYER_LAST_LAYER           /**<
		                            * Don't draw to this layer it's a dummy to
		                            * size the vector.
		                            */
		};

	/**
	 * Add an item to the drawing buffer. You need to update screen on affected area
	 *
	 * @param layer              The layer to draw on.
	 * @param loc                The hex the image belongs to, needed for the
	 *                           drawing order.
	 * @param blit               The structure to blit.
	 */
	void drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, const surface& surf, const int x, const int y, const int width, const int height,
			const SDL_Rect &clip = SDL_Rect());

	image::tblit& drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, const image::locator& loc2, const image::TYPE loc2_type, const int x, const int y, const int width, const int height,
			const SDL_Rect &clip = SDL_Rect());

	image::tblit& drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, const int type, const int x, const int y, const int width, const int height, const uint32_t color);

	void drawing_buffer_add(const tdrawing_layer layer,
			const map_location& loc, const int x, const int y,
			const std::vector<image::tblit>& blits);

	virtual SDL_Rect clip_rect_commit() const = 0;

	// Draws the drawing_buffer_ and clears it.
	void drawing_buffer_commit(texture& screen, const SDL_Rect& clip_rect);

	//
	// area animation secton
	//
	std::map<int, animation*>& area_anims() { return area_anims_; }
	animation& area_anim(int id);

	int insert_area_anim(const animation& tpl);
	int insert_area_anim2(animation* anim);
	virtual void erase_area_anim(int id);
	void clear_area_anims();

	// render relative
	/** Returns the current zoom factor. */
	virtual double get_zoom_factor() const = 0;

	/**
	 * Draw an image at a certain location.
	 * x,y: pixel location on screen to draw the image
	 * image: the image to draw
	 * reverse: if the image should be flipped across the x axis
	 * greyscale: used for instance to give the petrified appearance to a unit image
	 * alpha: the merging to use with the background
	 * blendto: blend to this color using blend_ratio
	 * submerged: the amount of the unit out of 1.0 that is submerged
	 *            (presumably under water) and thus shouldn't be drawn
	 */
	void render_image(int x, int y, const tdrawing_layer drawing_layer,
			const map_location& loc, image::tblit& image,
			bool hreverse=false, bool greyscale=false,
			fixed_t alpha=ftofxp(1.0), Uint32 blendto=0,
			double blend_ratio=0, double submerged=0.0,bool vreverse =false);

	void draw_text_in_hex2(const map_location& loc,
		const tdrawing_layer layer, const std::string& text,
		size_t font_size, SDL_Color color, int x, int y, fixed_t alpha = 255, bool center = false);

protected:
	/**
	 * In order to render a hex properly it needs to be rendered per row. On
	 * this row several layers need to be drawn at the same time. Mainly the
	 * unit and the background terrain. This is needed since both can spill
	 * in the next hex. The foreground terrain needs to be drawn before to
	 * avoid decapitation a unit.
	 *
	 * In other words:
	 * for every layer
	 *   for every row (starting from the top)
	 *     for every hex in the row
	 *       ...
	 *
	 * this is modified to:
	 * for every layer group
	 *   for every row (starting from the top)
	 *     for every layer in the group
	 *       for every hex in the row
	 *         ...
	 *
	 * * Surfaces are rendered per level in a map.
	 * * Per level the items are rendered per location these locations are
	 *   stored in the drawing order required for units.
	 * * every location has a vector with surfaces, each with its own screen
	 *   coordinate to render at.
	 * * every vector element has a vector with surfaces to render.
	 */
	class drawing_buffer_key
	{
	private:
		unsigned int key_;

		static const tdrawing_layer layer_groups[];
		static const unsigned int max_layer_group;

	public:
		drawing_buffer_key(const map_location &loc, tdrawing_layer layer);

		bool operator<(const drawing_buffer_key &rhs) const { return key_ < rhs.key_; }
	};

	/** Helper structure for rendering the terrains. */
	class tblit2
	{
	public:
		tblit2(const tdrawing_layer layer, const map_location& loc,
				const int x, const int y, const image::tblit& sloc)
			: x_(x), y_(y),
			key_(loc, layer)
		{
			surf_.push_back(sloc);
		}

		tblit2(const tdrawing_layer layer, const map_location& loc,
				const int x, const int y, const std::vector<image::tblit>& sloc)
			: x_(x)
			, y_(y)
			, surf_(sloc)
			, key_(loc, layer)
		{
		}

		int x() const { return x_; }
		int y() const { return y_; }
		const std::vector<image::tblit>& surf() const { return surf_; }
		std::vector<image::tblit>& surf() { return surf_; }

		bool operator<(const tblit2 &rhs) const { return key_ < rhs.key_; }

	private:
		int x_;                      /**< x screen coordinate to render at. */
		int y_;                      /**< y screen coordinate to render at. */
		std::vector<image::tblit> surf_;		/**< surface(s) to render. */
		drawing_buffer_key key_;
	};

	CVideo& video_;
	std::list<tblit2> drawing_buffer_;
	std::map<int, animation*> area_anims_;
};

#endif
