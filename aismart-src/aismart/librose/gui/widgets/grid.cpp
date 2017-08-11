/* $Id: grid.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/grid.hpp"

#include "gui/widgets/control.hpp"
#include "gui/widgets/spacer.hpp"
#include "gui/widgets/window.hpp"
#include "formula_string_utils.hpp"

#include <boost/foreach.hpp>

#include <numeric>

namespace gui2 {

tgrid::tgrid(const unsigned rows, const unsigned cols)
	: rows_(rows)
	, cols_(cols)
	, row_height_()
	, col_width_()
	, row_grow_factor_(rows)
	, col_grow_factor_(cols)
	, children_(NULL)
	, children_size_(rows * cols)
	, children_vsize_(rows * cols)
	, stuff_widget_()
	, valid_stuff_size_(0)
{
	if (children_size_) {
		children_ = (tchild*)malloc(children_size_ * sizeof(tchild));
		memset(children_, 0, children_size_ * sizeof(tchild)); 
	}
}

tgrid::~tgrid()
{
	if (children_) {
		for (int n = 0; n < children_vsize_; n ++) {
			delete children_[n].widget_;
			children_[n].widget_ = NULL;
		}
		free(children_);
	}
}

unsigned tgrid::add_row(const unsigned count)
{
	VALIDATE(count, null_str);

	// FIXME the warning in set_rows_cols should be killed.

	unsigned result = rows_;
	set_rows_cols(rows_ + count, cols_);
	return result;
}

void tgrid::set_row_grow_factor(const unsigned row, const unsigned factor)
{
	VALIDATE(row < row_grow_factor_.size(), null_str);
	row_grow_factor_[row] = factor;
	set_dirty();
}

void tgrid::set_column_grow_factor(const unsigned column, const unsigned factor)
{
	VALIDATE(column< col_grow_factor_.size(), null_str);
	col_grow_factor_[column] = factor;
	set_dirty();
}

void tgrid::set_child(twidget* widget, const unsigned row,
		const unsigned col, const unsigned flags, const unsigned border_size)
{
	VALIDATE(row < rows_ && col < cols_, null_str);
	VALIDATE(flags & VERTICAL_MASK, null_str);
	VALIDATE(flags & HORIZONTAL_MASK, null_str);

	tchild& cell = child(row, col);
/*
	// clear old child if any
	if (cell.widget_) {
		// free a child when overwriting it
		WRN_GUI_G << LOG_HEADER
				<< " child '" << cell.widget_->id()
				<< "' at cell '" << row << ',' << col
				<< "' will be replaced.\n";
		delete cell.widget_;
	}
*/
	VALIDATE(!cell.widget_, null_str);

	// copy data
	cell.flags_ = flags;
	cell.border_size_ = border_size;
	cell.widget_ = widget;
	if (cell.widget_) {
		// make sure the new child is valid before deferring
		cell.widget_->set_parent(this);
	}
}

twidget* tgrid::swap_child(
		const std::string& id, twidget* widget, const bool recurse,
		twidget* new_parent)
{
	assert(widget);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];
		if (child.widget_->id() != id) {
			if (recurse) {
				// decent in the nested grids.
				tgrid* grid = dynamic_cast<tgrid*>(child.widget_);
				if (grid) {

					twidget* old = grid->swap_child(id, widget, true);
					if (old) {
						return old;
					}
				}
			}

			continue;
		}

		// When find the widget there should be a widget.
		twidget* old = child.widget_;
		assert(old);
		old->set_parent(new_parent);

		widget->set_parent(this);
		child.widget_ = widget;

		return old;
	}

	return NULL;
}

void tgrid::remove_child(const unsigned row, const unsigned col)
{
	assert(row < rows_ && col < cols_);

	tchild& cell = child(row, col);

	if (cell.widget_) {
		delete cell.widget_;
	}
	cell.widget_ = NULL;
}

void tgrid::remove_child(const std::string& id, const bool find_all)
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		if (child.widget_->id() == id) {
			delete child.widget_;
			child.widget_ = NULL;

			if (!find_all) {
				break;
			}
		}
	}
}

void tgrid::resize_children(int size)
{
	if (size > children_size_) {
		do {
			children_size_ += children_size_? children_size_ * 2: 10;
		} while (size > children_size_);
		tchild* tmp = (tchild*)malloc(children_size_ * sizeof(tchild));
		memset(tmp, 0, children_size_ * sizeof(tchild)); 
		if (children_) {
			if (children_vsize_) {
				memcpy(tmp, children_, children_vsize_ * sizeof(tchild));
			}
			free(children_);
		}
		children_ = tmp;
	}
}

int tgrid::children_vsize() const
{
	return children_vsize_ - stuff_widget_.size();
}

void tgrid::set_active(const bool active)
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		if (!widget) {
			continue;
		}

		tgrid* grid = dynamic_cast<tgrid*>(widget);
		if (grid) {
			grid->set_active(active);
			continue;
		}

		tcontrol* control =  dynamic_cast<tcontrol*>(widget);
		if(control) {
			control->set_active(active);
		}
	}
}

bool tgrid::has_child_visible() const
{
	for (int n = 0; n < children_vsize_; n ++) {
		const tgrid::tchild& child = children_[n];

		if (child.widget_->get_visible() == twidget::VISIBLE) {
			return true;
		}
	}
	return false;
}

void tgrid::layout_init(bool linked_group_only)
{
	// Inherited.
	twidget::layout_init(linked_group_only);

	// Clear child caches.
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		if (child.widget_->get_visible() != twidget::INVISIBLE) {
			child.widget_->layout_init(linked_group_only);
		}
	}
}

tpoint tgrid::calculate_best_size() const
{
	// Reset the cached values.
	row_height_.clear();
	row_height_.resize(rows_, 0);
	col_width_.clear();
	col_width_.resize(cols_, 0);

	// First get the sizes for all items.
	for (unsigned row = 0; row < rows_; ++row) {
		for (unsigned col = 0; col < cols_; ++col) {

			const tpoint size = cell_get_best_size(child(row, col));

			if (size.x > static_cast<int>(col_width_[col])) {
				col_width_[col] = size.x;
			}

			if (size.y > static_cast<int>(row_height_[row])) {
				row_height_[row] = size.y;
			}
		}
	}

	const tpoint result(
		std::accumulate(col_width_.begin(), col_width_.end(), 0),
		std::accumulate(row_height_.begin(), row_height_.end(), 0));

	return result;
}

tpoint tgrid::calculate_best_size_bh(const int width)
{
	// call calculate_best_size just, below statement must be true.
	VALIDATE(row_height_.size() == rows_ && col_width_.size() == cols_, null_str);

/*
	if (row_height_.size() != rows_ || col_width_.size() != cols_) {
		const tpoint result(std::accumulate(col_width_.begin(), col_width_.end(), 0), std::accumulate(row_height_.begin(), row_height_.end(), 0));
		return result;
	}

*/

	tpoint ret(0, 0);

	for (unsigned row = 0; row < rows_; ++row) {
		VALIDATE(col_width_.size() == cols_, null_str);

		if (cols_ == 1) {
			const tchild& cell = child(row, 0);
			if (cell.widget_->get_visible() == twidget::INVISIBLE) {
				continue;
			}

			ret = cell.widget_->calculate_best_size_bh(width - cell_border_space(cell).x);

			if (ret.x != nposm) {
				ret += cell_border_space(cell);

				// col_width_[0] is this column's maxmin width, ret.x may be less it. 
				// as if after calculate_best_size, multiline label's width must be 0.

				if (ret.x > (int)col_width_[0]) {
					col_width_[0] = ret.x;
				}
				row_height_[row] = ret.y;
			}

		} else {
			int best_width = 0;
			unsigned w_size = 0;
			int cols = 0;
			for (unsigned col = 0; col < cols_; ++col) {
				const tchild& cell = child(row, col);
				if (cell.widget_->get_visible() == twidget::INVISIBLE) {
					continue;
				}
			
				best_width += col_width_[col];
				w_size += col_grow_factor_[col];
				cols ++;
			}
			if (!cols) {
				continue;
			}
			const unsigned w = width - best_width;
			if (w_size == 0) {
				// If all sizes are 0 reset them to 1
				w_size = cols;
			}
			for (unsigned col = 0; col < cols_; ++col) {
				const tchild& cell = child(row, col);
				if (cell.widget_->get_visible() == twidget::INVISIBLE) {
					continue;
				}

				// We might have a bit 'extra' if the division doesn't fix exactly
				// but we ignore that part for now.
				const unsigned w_normal = w / w_size;

				unsigned placeable_width = col_width_[col] + w_normal * col_grow_factor_[col];
				ret = cell.widget_->calculate_best_size_bh(placeable_width - cell_border_space(cell).x);

				if (ret.x != nposm) {
					ret += cell_border_space(cell);

					// col_width_[col] is this column's maxmin width, ret.x may be less it. 
					// as if after calculate_best_size, multiline label's width must be 0.

					if (ret.x > (int)col_width_[col]) {
						col_width_[col] = ret.x;
					}
					if (ret.y > (int)row_height_[row]) {
						row_height_[row] = ret.y;
					}
				}
			}
		}
	}

	const tpoint result(
		std::accumulate(col_width_.begin(), col_width_.end(), 0),
		std::accumulate(row_height_.begin(), row_height_.end(), 0));

	return result;
}

void tgrid::place(const tpoint& origin, const tpoint& size)
{
	/***** INIT *****/

	twidget::place(origin, size);

	if (!rows_ || !cols_) {
		return;
	}

	// call the calculate so the size cache gets updated.
	tpoint best_size = calculate_best_size();

	VALIDATE(row_height_.size() == rows_, null_str);
	VALIDATE(col_width_.size() == cols_, null_str);

	VALIDATE(row_grow_factor_.size() == rows_, null_str);
	VALIDATE(col_grow_factor_.size() == cols_, null_str);

	if (best_size == size) {
		layout(origin);
		return;
	}

	/***** GROW *****/
	if (best_size.x <= size.x && best_size.y <= size.y) {
		// expand it.
		if (size.x > best_size.x) {
			std::pair<int, int> restrict_width_factor = std::make_pair(nposm, nposm);
			if (cols_ > 1) {
				// reference to http://www.libsdl.cn/bbs/forum.php?mod=viewthread&tid=133&extra=page%3D1%26filter%3Dtypeid%26typeid%3D21
				for (int at = 0; at < children_vsize_; at ++) {
					if (children_[at].widget_->get_visible() == twidget::INVISIBLE) {
						continue;
					}
					if (children_[at].widget_->restrict_width() && col_width_[at % cols_]) {
						// if previous calculated width_ is 0, use col_grow_factor continue. 
						// there is one restrict_width widget at most.
						// this grid maybe more rows using multi-line label.
						VALIDATE(restrict_width_factor.first == nposm || (restrict_width_factor.first % cols_ == at % cols_), null_str);

						tcontrol* widget = dynamic_cast<tcontrol*>(children_[at].widget_);
						if (!widget->label().empty()) {
							const int new_best_width = widget->get_multiline_best_width();
							best_size.x += new_best_width - col_width_[at % cols_];
							col_width_[at % cols_] = widget->get_multiline_best_width();
							restrict_width_factor = std::make_pair(at, col_grow_factor_[at % cols_]);
						}
					}
				}
			}
			const unsigned w = size.x - best_size.x;
			unsigned w_size = std::accumulate(
					col_grow_factor_.begin(), col_grow_factor_.end(), 0);
			if (restrict_width_factor.first != nposm) {
				w_size -= restrict_width_factor.second;
			}
			// extra width(w) will be divided amount (w_size) units in (cols_) columns.
			if (w_size == 0) {
				if (restrict_width_factor.first == nposm) {
					// If all sizes are 0 reset them to 1
					BOOST_FOREACH(unsigned& val, col_grow_factor_) {
						val = 1;
					}
				}
				w_size = cols_;
				if (restrict_width_factor.first != nposm) {
					w_size --;
				}
			}
			// We might have a bit 'extra' if the division doesn't fix exactly
			// but we ignore that part for now.
			const unsigned w_normal = w / w_size;
			for (unsigned i = 0; i < cols_; ++i) {
				if (restrict_width_factor.first == i) {
					continue;
				}
				col_width_[i] += w_normal * col_grow_factor_[i];
				// column(i) with grow factor (col_grow_factor_[i]) set width to (col_width_[i]).
			}

		}

		if (size.y > best_size.y) {
			const unsigned h = size.y - best_size.y;
			unsigned h_size = std::accumulate(
					row_grow_factor_.begin(), row_grow_factor_.end(), 0);
			
			// extra height(h) will be divided amount(h_size) units in (rows_) rows.
			if (h_size == 0) {
				// If all sizes are 0 reset them to 1
				BOOST_FOREACH(unsigned& val, row_grow_factor_) {
					val = 1;
				}
				h_size = rows_;
			}
			// We might have a bit 'extra' if the division doesn't fix exactly
			// but we ignore that part for now.
			const unsigned h_normal = h / h_size;
			for (unsigned i = 0; i < rows_; ++i) {
				row_height_[i] += h_normal * row_grow_factor_[i];
				// row(i) with grow factor(row_grow_factor_[i]) set height to (row_height_[i]).
			}
		}

		VALIDATE(std::accumulate(col_width_.begin(), col_width_.end(), 0) <= size.x, null_str);
		VALIDATE(std::accumulate(row_height_.begin(), row_height_.end(), 0) <= size.y, null_str);

		layout(origin);
		return;
	}

	// This shouldn't be possible...
	utils::string_map symbols;
	std::stringstream tmp;
	if (!parent()->id().empty()) {
		tmp << parent()->id();
	} else {
		tmp << "--";
	}
	symbols["parent"] = ht::generate_format(tmp.str(), 0xffff0000);
	tmp.str("");
	tmp << "(" << size.x << "," << size.y << ")";
	symbols["layout_size"] = ht::generate_format(tmp.str(), 0xffff0000);
	tmp.str("");
	tmp << "(" << best_size.x << "," << best_size.y << ")";
	symbols["best_size"] = ht::generate_format(tmp.str(), 0xffff0000);

	std::string err = vgettext2("Place grid fail! It's parent widget: $parent\n\nLayout size that calcualted base best size and grow_factor: $layout_size\nNow recalculated best size: $best_size", symbols);
	throw twindow::tlayout_exception(*this->get_window(), err);
}

void tgrid::set_origin(const tpoint& origin)
{
	const tpoint movement = tpoint(origin.x - get_x(), origin.y - get_y());

	// Inherited.
	twidget::set_origin(origin);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		VALIDATE(widget, null_str);

		widget->set_origin(tpoint(
				widget->get_x() + movement.x,
				widget->get_y() + movement.y));
	}
}

void tgrid::set_visible_area(const SDL_Rect& area)
{
	// Inherited.
	twidget::set_visible_area(area);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;

		widget->set_visible_area(area);
	}
}

void tgrid::layout_children()
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];
		child.widget_->layout_children();
	}
}

void tgrid::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	VALIDATE(!call_stack.empty() && call_stack.back() == this, null_str);

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		std::vector<twidget*> child_call_stack = call_stack;
		child.widget_->populate_dirty_list(caller, child_call_stack);
	}
}

bool tgrid::popup_new_window()
{
	bool require_redraw = false;
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		require_redraw |= child.widget_->popup_new_window();
	}
	return require_redraw;
}

void tgrid::dirty_under_rect(const SDL_Rect& clip)
{
	if (visible_ != VISIBLE) {
		return;
	}
	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];

		VALIDATE(child.widget_, null_str);
		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		child.widget_->dirty_under_rect(clip);
	}
}

twidget* tgrid::find_at(const tpoint& coordinate, const bool must_be_active)
{
	if (visible_ != VISIBLE) {
		return nullptr;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];

		VALIDATE(child.widget_, null_str);
		if (child.widget_->get_visible() != VISIBLE) {
			continue;
		}

		twidget* widget = child.widget_->find_at(coordinate, must_be_active);
		if (widget) {
			return widget;
		}
	}
	return nullptr;
}

twidget* tgrid::find(const std::string& id, const bool must_be_active)
{
	twidget* widget = twidget::find(id, must_be_active);
	if (widget) {
		return widget;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];
		widget = child.widget_->find(id, must_be_active);
		if (widget) {
			return widget;
		}
	}
	return nullptr;
}

const twidget* tgrid::find(const std::string& id,
		const bool must_be_active) const
{
	const twidget* widget = twidget::find(id, must_be_active);
	if (widget) {
		return widget;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		widget = child.widget_->find(id, must_be_active);
		if (widget) {
			return widget;
		}
	}
	return nullptr;
}

bool tgrid::has_widget(const twidget* widget) const
{
	if(twidget::has_widget(widget)) {
		return true;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		if (child.widget_->has_widget(widget)) {
			return true;
		}
	}
	return false;
}

bool tgrid::disable_click_dismiss() const
{
	if(get_visible() != twidget::VISIBLE) {
		return false;
	}

	for (int n = 0; n < children_vsize_; n ++) {
		const tchild& child = children_[n];
		const twidget* widget = child.widget_;
		assert(widget);

		if(widget->disable_click_dismiss()) {
			return true;
		}
	}
	return false;
}

void tgrid::set_rows(const unsigned rows)
{
	if(rows == rows_) {
		return;
	}

	set_rows_cols(rows, cols_);
}

void tgrid::set_cols(const unsigned cols)
{
	if(cols == cols_) {
		return;
	}

	set_rows_cols(rows_, cols);
}

void tgrid::set_rows_cols(const unsigned rows, const unsigned cols)
{
	if (rows == rows_ && cols == cols_) {
		return;
	}

	VALIDATE(!children_, null_str);

	rows_ = rows;
	cols_ = cols;
	row_grow_factor_.resize(rows);
	col_grow_factor_.resize(cols);
	resize_children((rows * cols) + 1);
	children_vsize_ = rows * cols;
}

tpoint cell_get_best_size(const tgrid::tchild& cell)
{
	if (!cell.widget_) {
		return cell_border_space(cell);
	}

	if (cell.widget_->get_visible() == twidget::INVISIBLE) {
		return tpoint(0, 0);
	}

	const tpoint best_size = cell.widget_->get_best_size() + cell_border_space(cell);
	return best_size;
}

void cell_place(tgrid::tchild& cell, tpoint origin, tpoint size)
{
	if (cell.widget_->get_visible() == twidget::INVISIBLE) {
		return;
	}

	if (cell.border_size_) {
		if (cell.flags_ & tgrid::BORDER_TOP) {
			origin.y += cell.border_size_;
			size.y -= cell.border_size_;
		}
		if (cell.flags_ & tgrid::BORDER_BOTTOM) {
			size.y -= cell.border_size_;
		}

		if (cell.flags_ & tgrid::BORDER_LEFT) {
			origin.x += cell.border_size_;
			size.x -= cell.border_size_;
		}
		if (cell.flags_ & tgrid::BORDER_RIGHT) {
			size.x -= cell.border_size_;
		}
	}

	// If size smaller or equal to best size set that size.
	// No need to check > min size since this is what we got.
	const tpoint best_size = cell.widget_->get_best_size();
	if (size <= best_size) {
		cell.widget_->place(origin, size);
		return;
	}

	const tcontrol* control = dynamic_cast<const tcontrol*>(cell.widget_);

	if ((cell.flags_ & (tgrid::HORIZONTAL_MASK | tgrid::VERTICAL_MASK)) == (tgrid::HORIZONTAL_ALIGN_EDGE | tgrid::VERTICAL_ALIGN_EDGE)) {
		cell.widget_->place(origin, size);
		return;
	}

	tpoint widget_size(std::min(size.x, best_size.x), std::min(size.y, best_size.y));
	tpoint widget_orig = origin;

	const unsigned v_flag = cell.flags_ & tgrid::VERTICAL_MASK;

	if (v_flag == tgrid::VERTICAL_ALIGN_EDGE) {
		widget_size.y = size.y;

	} else if (v_flag == tgrid::VERTICAL_ALIGN_TOP) {
		// Do nothing.

	} else if (v_flag == tgrid::VERTICAL_ALIGN_CENTER) {

		widget_orig.y += (size.y - widget_size.y) / 2;

	} else if (v_flag == tgrid::VERTICAL_ALIGN_BOTTOM) {

		widget_orig.y += (size.y - widget_size.y);

	} else {
		// err << " Invalid vertical alignment '" << v_flag << "' specified."
		VALIDATE(false, null_str);
	}

	
	const unsigned h_flag = cell.flags_ & tgrid::HORIZONTAL_MASK;

	if (h_flag == tgrid::HORIZONTAL_ALIGN_EDGE) {
		widget_size.x = size.x;

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_LEFT) {
		// Do nothing.

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_CENTER) {

		widget_orig.x += (size.x - widget_size.x) / 2;

	} else if (h_flag == tgrid::HORIZONTAL_ALIGN_RIGHT) {

		widget_orig.x += (size.x - widget_size.x);

	} else {
		// err << " No horizontal alignment '" << h_flag << "' specified.";
		VALIDATE(false, null_str);
	}

	cell.widget_->place(widget_orig, widget_size);
}

tpoint cell_border_space(const tgrid::tchild& cell)
{
	tpoint result(0, 0);

	if(cell.border_size_) {

		if(cell.flags_ & tgrid::BORDER_TOP) result.y += cell.border_size_;
		if(cell.flags_ & tgrid::BORDER_BOTTOM) result.y += cell.border_size_;

		if(cell.flags_ & tgrid::BORDER_LEFT) result.x += cell.border_size_;
		if(cell.flags_ & tgrid::BORDER_RIGHT) result.x += cell.border_size_;
	}

	return result;
}

void tgrid::layout(const tpoint& origin)
{
	tpoint orig = origin;
	for(unsigned row = 0; row < rows_; ++row) {
		for(unsigned col = 0; col < cols_; ++col) {

			const tpoint size(col_width_[col], row_height_[row]);
			// set widget at " << row << ',' << col
			// at origin " << orig
			// with size " << size

			if (child(row, col).widget_) {
				cell_place(child(row, col), orig, size);
			}

			orig.x += col_width_[col];
		}
		orig.y += row_height_[row];
		orig.x = origin.x;
	}
}

void tgrid::impl_draw_children(
		  texture& frame_buffer
		, int x_offset
		, int y_offset)
{
	VALIDATE(get_visible() == twidget::VISIBLE, null_str);
	clear_dirty();

	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
        VALIDATE(widget, null_str);

		if (widget->get_visible() != twidget::VISIBLE) {
			if (widget->get_visible() == twidget::HIDDEN) {
				widget->clear_dirty();
			}
			continue;
		}

		if (widget->get_drawing_action() == twidget::NOT_DRAWN) {
			continue;
		}

		widget->draw_background(frame_buffer, x_offset, y_offset);
		widget->draw_children(frame_buffer, x_offset, y_offset);
		widget->clear_dirty();
	}
}

void tgrid::broadcast_frame_buffer(texture& frame_buffer)
{
	for (int n = 0; n < children_vsize_; n ++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		widget->broadcast_frame_buffer(frame_buffer);
	}
}

void tgrid::clear_texture()
{
	for (int n = 0; n < children_vsize_; n++) {
		tchild& child = children_[n];

		twidget* widget = child.widget_;
		if (widget) {
			widget->clear_texture();
		}
	}
}

bool tgrid::canvas_texture_is_null() const
{
	for (int n = 0; n < children_vsize_; n ++) {
		const tcontrol* control = dynamic_cast<tcontrol*>(children_[n].widget_);
		if (control && !control->canvas().empty()) {
			const tcanvas& canvas = control->canvas(control->get_state());
			const texture& target = canvas.canvas_tex();
			if (target.get()) {
				return false;
			}
		}
	}
	return true;
}

const std::string& tgrid::get_control_type() const
{
	static const std::string type = "grid";
	return type;
}

} // namespace gui2


/*WIKI
 * @page = GUILayout
 *
 * {{Autogenerated}}
 *
 * = Abstract =
 *
 * In the widget library the placement and sizes of elements is determined by
 * a grid. Therefore most widgets have no fixed size.
 *
 *
 * = Theory =
 *
 * We have two examples for the addon dialog, the first example the lower
 * buttons are in one grid, that means if the remove button gets wider
 * (due to translations) the connect button (4.1 - 2.2) will be aligned
 * to the left of the remove button. In the second example the connect
 * button will be partial underneath the remove button.
 *
 * A grid exists of x rows and y columns for all rows the number of columns
 * needs to be the same, there is no column (nor row) span. If spanning is
 * required place a nested grid to do so. In the examples every row has 1 column
 * but rows 3, 4 (and in the second 5) have a nested grid to add more elements
 * per row.
 *
 * In the grid every cell needs to have a widget, if no widget is wanted place
 * the special widget ''spacer''. This is a non-visible item which normally
 * shouldn't have a size. It is possible to give a spacer a size as well but
 * that is discussed elsewhere.
 *
 * Every row and column has a ''grow_factor'', since all columns in a grid are
 * aligned only the columns in the first row need to define their grow factor.
 * The grow factor is used to determine with the extra size available in a
 * dialog. The algorithm determines the extra size work like this:
 *
 * * determine the extra size
 * * determine the sum of the grow factors
 * * if this sum is 0 set the grow factor for every item to 1 and sum to sum of items.
 * * divide the extra size with the sum of grow factors
 * * for every item multiply the grow factor with the division value
 *
 * eg
 *  extra size 100
 *  grow factors 1, 1, 2, 1
 *  sum 5
 *  division 100 / 5 = 20
 *  extra sizes 20, 20, 40, 20
 *
 * Since we force the factors to 1 if all zero it's not possible to have non
 * growing cells. This can be solved by adding an extra cell with a spacer and a
 * grow factor of 1. This is used for the buttons in the examples.
 *
 * Every cell has a ''border_size'' and ''border'' the ''border_size'' is the
 * number of pixels in the cell which aren't available for the widget. This is
 * used to make sure the items in different cells aren't put side to side. With
 * ''border'' it can be determined which sides get the border. So a border is
 * either 0 or ''border_size''.
 *
 * If the widget doesn't grow when there's more space available the alignment
 * determines where in the cell the widget is placed.
 *
 * == Examples ==
 *
 *  |---------------------------------------|
 *  | 1.1                                   |
 *  |---------------------------------------|
 *  | 2.1                                   |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 3.1 - 1.1          | 3.1 - 1.2    | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 4.1 - 1.1 | 4.1 - 1.2 | 4.1 - 1.3 | |
 *  | |-----------------------------------| |
 *  | | 4.1 - 2.1 | 4.1 - 2.2 | 4.1 - 2.3 | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *
 *
 *  1.1       label : title
 *  2.1       label : description
 *  3.1 - 1.1 label : server
 *  3.1 - 1.2 text box : server to connect to
 *  4.1 - 1.1 spacer
 *  4.1 - 1.2 spacer
 *  4.1 - 1.3 button : remove addon
 *  4.2 - 2.1 spacer
 *  4.2 - 2.2 button : connect
 *  4.2 - 2.3 button : cancel
 *
 *
 *  |---------------------------------------|
 *  | 1.1                                   |
 *  |---------------------------------------|
 *  | 2.1                                   |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 3.1 - 1.1          | 3.1 - 1.2    | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 4.1 - 1.1         | 4.1 - 1.2     | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *  | |-----------------------------------| |
 *  | | 5.1 - 1.1 | 5.1 - 1.2 | 5.1 - 2.3 | |
 *  | |-----------------------------------| |
 *  |---------------------------------------|
 *
 *
 *  1.1       label : title
 *  2.1       label : description
 *  3.1 - 1.1 label : server
 *  3.1 - 1.2 text box : server to connect to
 *  4.1 - 1.1 spacer
 *  4.1 - 1.2 button : remove addon
 *  5.2 - 1.1 spacer
 *  5.2 - 1.2 button : connect
 *  5.2 - 1.3 button : cancel
 *
 * = Praxis =
 *
 * This is the code needed to create the skeleton for the structure the extra
 * flags are ommitted.
 *
 *  	[grid]
 *  		[row]
 *  			[column]
 *  				[label]
 *  					# 1.1
 *  				[/label]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[label]
 *  					# 2.1
 *  				[/label]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[grid]
 *  					[row]
 *  						[column]
 *  							[label]
 *  								# 3.1 - 1.1
 *  							[/label]
 *  						[/column]
 *  						[column]
 *  							[text_box]
 *  								# 3.1 - 1.2
 *  							[/text_box]
 *  						[/column]
 *  					[/row]
 *  				[/grid]
 *  			[/column]
 *  		[/row]
 *  		[row]
 *  			[column]
 *  				[grid]
 *  					[row]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 1.1
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 1.2
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 1.3
 *  							[/button]
 *  						[/column]
 *  					[/row]
 *  					[row]
 *  						[column]
 *  							[spacer]
 *  								# 4.1 - 2.1
 *  							[/spacer]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 2.2
 *  							[/button]
 *  						[/column]
 *  						[column]
 *  							[button]
 *  								# 4.1 - 2.3
 *  							[/button]
 *  						[/column]
 *  					[/row]
 *  				[/grid]
 *  			[/column]
 *  		[/row]
 *  	[/grid]
 *
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 */
