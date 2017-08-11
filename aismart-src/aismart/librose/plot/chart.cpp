#define GETTEXT_DOMAIN "rose-lib"

#include "plot/chart.hpp"
#include "wml_exception.hpp"

int tplot_chart::calculate_pixel_temperature(bool slope, int number, const int pixels, const int last_unit_value, time_t base_time, int duration, int samples, const titem* chart_data, int* result, int& writable_pos)
{
	int time_per_pixel = 0, pixel_per_time = 0;
	int can_spread_time = 0, can_spread_pixels = 0;
	int* result_ptr = result + writable_pos;
	int last_value = last_unit_value;

	int n = 0, at;
	if (duration >= pixels) {
		time_per_pixel = duration / pixels;
		can_spread_time = duration - time_per_pixel * pixels;

	} else if (duration) {
		pixel_per_time = pixels / duration;
		can_spread_pixels = pixels - pixel_per_time * duration;

	} else {
		VALIDATE(!slope && !samples, null_str);
		memset(result, 0, sizeof(int) * pixels);
		writable_pos += pixels;
		return 0;
	}

	SDL_Rect r = empty_rect;
	int value;
	bool key;
	time_t end_time = base_time;
	const titem* t2 = NULL;

	for (int pixel = 0; pixel < pixels; ) {
		if (time_per_pixel) {
			int time_per_pixel2 = time_per_pixel;
			if (can_spread_time) {
				time_per_pixel2 ++;
				can_spread_time --;
			}

			base_time = end_time;
			end_time = base_time + time_per_pixel2;

		} else {
			base_time = end_time;
			end_time = base_time + 1;
		}

		int pixel_per_time2 = pixel_per_time;
		if (can_spread_pixels) {
			pixel_per_time2 ++;
			can_spread_pixels --;
		}

		key = true;
		value = nposm;
		while (t2 || n < samples) {
			if (!t2) {
				t2 = (const titem*)((uint8_t*)chart_data + n * item_size_);
				n ++;
			}
			if (t2->time >= end_time) {
				break;
			}
			value = calculate_value(*t2, value);
			t2 = NULL;
		}
		if (slope) {
			if (value == nposm) {
				if (time_per_pixel) {
					pixel ++;
				} else {
					pixel += pixel_per_time2;
				}
				continue;
			}
/*
			if (value >= MIN_TEMPERATURE) {
				if (value > MAX_TEMPERATURE) {
					value = MAX_TEMPERATURE;
				}
			} else {
				value = MIN_TEMPERATURE;
			}
*/
			VALIDATE(duration, null_str);

			int stop_pos = number * pixels + pixel; // last pixel is exact value.
			double increase = 1.0 * (value - last_value) / (stop_pos - writable_pos);
			for (at = writable_pos; at < stop_pos; at ++) {
				*result_ptr = last_value + (at - writable_pos) * increase;
				result_ptr ++;
			}
			VALIDATE(result_ptr == result + number * pixels + pixel, null_str);
			*result_ptr = value | 0x10000;
			result_ptr ++;

			writable_pos = stop_pos + 1;
			if (time_per_pixel) {
				pixel ++;
			} else {
				pixel += pixel_per_time2;
			}

		} else {
			if (value == nposm) {
				key = false;
				value = last_value;
			}
			value = clip_value(value);

			value |= (key? 0x10000: 0);
			if (time_per_pixel) {
				*result_ptr = value;
				result_ptr ++;

				pixel ++;

			} else {
				while (pixel_per_time2 --) {
					*result_ptr = value;
					result_ptr ++;

					pixel ++;
				}
			}

		}
		last_value = value & 0xffff;
	}

	VALIDATE(n == samples, null_str);

	if (!slope) {
		writable_pos += pixels;
	}
	VALIDATE(result_ptr == result + writable_pos, null_str);

	return last_value;
}

void tplot_chart::resize_data_per_unit(int size)
{
	VALIDATE(size > 0, null_str);

	if (size > (int)(data_per_unit_size * item_size_)) {
		titem* tmp = (titem*)malloc(size);
		if (data_per_unit) {
			if (data_per_unit_size) {
				memcpy(tmp, data_per_unit, data_per_unit_size * item_size_);
			}
			free(data_per_unit);
		}
		data_per_unit = tmp;
		data_per_unit_size = size / item_size_;
	}
}

void tplot_chart::generate(tfile& lock, int used_units, int zoom)
{
/*
	if (chart_data_key.first == used_units && chart_data_key.second == zoom) {
		return;
	}
*/
	VALIDATE(chart_data2, null_str);

	release_vector();

	// whether sample count < unit count or not. when slope = true, mean at least one unit hasn't sample, appear slope.
	bool slope = true;
	const int default_data_per_unit_size = 1024;
	if (!data_per_unit_size) {
		VALIDATE(!data_per_unit, null_str);
		// initial 1024 data, it maybe extend when draw.
		data_per_unit_size = default_data_per_unit_size;
		data_per_unit = (titem*)malloc(data_per_unit_size * item_size_);
	}
	VALIDATE(used_units * zoom <= max_chart_data_size_, null_str);

	int pixels_per_unit = zoom;
	const int duration2 = duration();
	int time_per_unit = duration2 / used_units;
	int can_spread_time = duration2 - time_per_unit * used_units;

	if (!time_per_unit) {
		slope = false;
	}

	int x = 0;

	int64_t last_fread_byte = 0; // data pos, not file pos!

	const int bytes_per_sample = item_size_;
	const int one_read_bytes = 4096 * bytes_per_sample;
	int last_unit_lack_samples = 0;

	lock.resize_data(one_read_bytes);
	posix_fseek(lock.fp, data_offset());

	char* data_ptr = NULL;

	const int items2 = items();
	const int64_t data_size2 = data_size();
	VALIDATE(items2 * item_size_ == data_size2, null_str);

	int writable_pos = 0;
	int start_samples = 0, temperature_samples, event_samples = 0, last_unit_value = 0;
	int64_t parsed_size = 0;
	int64_t last_parsed_size = parsed_size;
	int* chart_data2_ptr = chart_data2;
	time_t base_time, end_time = first_time();
	titem* t2 = create_item();
	t2->time = nposm;
	std::vector<tevent*> events;
	time_t* start_times = (time_t*)malloc(sizeof(time_t) * used_units);
	for (int num = 0; num < used_units; num ++) {
		int time_per_unit2 = time_per_unit;
		if (can_spread_time) {
			time_per_unit2 ++;
			can_spread_time --;
		}
		if (num == used_units - 1) {
			// if it is last unit, its duration require plus 1, so include last sample.
			time_per_unit2 ++;
		}
		base_time = end_time;
		end_time = base_time + time_per_unit2;
		temperature_samples = 0;
		
		while (start_samples + temperature_samples + event_samples < items2) {
			if (t2->time == nposm) {
				if (!data_ptr || data_ptr == lock.data + last_fread_byte) {
					int size = one_read_bytes;
					if (data_size2 - parsed_size < one_read_bytes) {
						size = data_size2 - parsed_size;
					}
					last_fread_byte = posix_fread(lock.fp, lock.data, size);
					data_ptr = lock.data;
				}

				const titem* t = (titem*)data_ptr;
				parsed_size += bytes_per_sample;
				data_ptr += bytes_per_sample;

				if (is_event_item(*t)) {
					std::pair<int, int> pair = disassemble_alert(*t);
					events.push_back(create_event(pair.first, t->time, pair.second));
					event_samples ++;
					continue;
				}

				memcpy(t2, t, item_size_);
			}

			if (t2->time >= end_time) {
				break;
			}

			memcpy((uint8_t*)data_per_unit + temperature_samples * item_size_, t2, item_size_);
			temperature_samples ++;

			if (temperature_samples == data_per_unit_size) {
				resize_data_per_unit((data_per_unit_size + default_data_per_unit_size / 2) * item_size_);
			}

			t2->time = nposm;
		}

		last_unit_value = calculate_pixel_temperature(slope, num, pixels_per_unit, last_unit_value, base_time, time_per_unit2, temperature_samples, data_per_unit, chart_data2, writable_pos);
		unit_params.push_back(tunit_param(base_time, time_per_unit2, start_samples, temperature_samples));
		start_times[num] = base_time;
		
		start_samples += temperature_samples;
		chart_data2_ptr += pixels_per_unit;
		// last_unit_value = *(chart_data2_ptr - 1) & 0xffff;

		x += pixels_per_unit;
		last_parsed_size = parsed_size;
	}

	// timestamp of event maybe not sequence with data. special evalue.
	for (std::vector<tevent*>::const_iterator it = events.begin(); it != events.end(); ++ it) {
		tevent& e = **it;
		for (int num = used_units - 1; num >= 0; num --) {
			if (e.time >= start_times[num]) {
				tunit_param& param = unit_params[num];
				param.events.push_back(&e);
				break;
			}
		}
	}

	if (writable_pos != used_units * zoom) {
		int remainer = used_units * zoom - writable_pos;
/*
		VALIDATE(slope && remainer < zoom, null_str);

		int* result_ptr = chart_data2 + writable_pos;
		last_unit_value = *(result_ptr - 1) & 0xffff;
		while (remainer --) {
			*result_ptr = last_unit_value;
			result_ptr ++;
		}
*/
	}

	VALIDATE(parsed_size == data_size2, null_str);
	VALIDATE(chart_data2_ptr == chart_data2 + (used_units * zoom), null_str);
	VALIDATE(unit_params.size() == used_units, null_str);

	free(t2);
	free(start_times);
	// rember this key.
	// chart_data_key.first = used_units;
	// chart_data_key.second = zoom;
}
