#ifndef LIBROSE_CHART_HPP_INCLUDED
#define LIBROSE_CHART_HPP_INCLUDED

#include "sdl_utils.hpp"
#include "filesystem.hpp"
#include "gui/widgets/widget.hpp"

class tplot_chart
{
public:
	struct titem {
		int time;
	};

	struct tevent {
		tevent(int type, time_t time, int value)
			: type(type)
			, time(time)
			, value(value)
		{}

		int type;
		time_t time;
		int value;
	};

	struct tunit_param {
		// events is evalated later.
		tunit_param(time_t base_time, int duration, int start_samples, int samples)
			: base_time(base_time)
			, duration(duration)
			, start_samples(start_samples)
			, samples(samples)
			, events()
		{}

		time_t base_time;
		int duration;
		int start_samples;
		int samples;
		std::vector<tevent*> events;
	};

	tplot_chart(int max_chart_data_size, int bytes_per_pixel_data, int item_size)
		: data_per_unit(NULL)
		, data_per_unit_size(0)
		, chart_data2(NULL)
		, max_chart_data_size_(max_chart_data_size)
		, bytes_per_pixel_data_(bytes_per_pixel_data)
		, item_size_(item_size)
	{
		if (max_chart_data_size_ * bytes_per_pixel_data_) {
			chart_data2 = (int*)malloc(max_chart_data_size_ * bytes_per_pixel_data_);
		}
	}

	virtual ~tplot_chart()
	{
		release_vector();
		if (data_per_unit) {
			delete data_per_unit;
			data_per_unit = NULL;
		}
		if (chart_data2) {
			delete chart_data2;
			chart_data2 = NULL;
		}
	}

	void generate(tfile& lock, int used_units, int zoom);

protected:
	void resize_data_per_unit(int size);

private:
	void release_vector()
	{
		for (std::vector<tevent*>::const_iterator it = events.begin(); it != events.end(); ++ it) {
			delete *it;
		}
		events.clear();

		unit_params.clear();
	}
	int calculate_pixel_temperature(bool slope, int number, const int pixels, const int last_unit_value, time_t base_time, int duration, int samples, const titem* chart_data, int* result, int& writable_pos);

	virtual int clip_value(int value) const { return value; }
	virtual int calculate_value(const titem& item, const int history) = 0;

	virtual bool is_event_item(const titem& item) const = 0;
	virtual std::pair<int, int> disassemble_alert(const titem& item) const = 0;

	virtual int items() const = 0;
	virtual int duration() const  = 0;
	virtual int64_t data_offset() const = 0;
	virtual int64_t data_size() const = 0;
	virtual time_t first_time() const = 0;

	virtual titem* create_item() const = 0;
	virtual tevent* create_event(int type, time_t time, int value) const = 0;

public:
	std::pair<int, int> chart_data_key;
	std::vector<tunit_param> unit_params;
	std::vector<tevent*> events;
	titem* data_per_unit;
	int data_per_unit_size;

	int* chart_data2;

protected:
	int max_chart_data_size_;
	int bytes_per_pixel_data_;
	int item_size_;
};


#endif
