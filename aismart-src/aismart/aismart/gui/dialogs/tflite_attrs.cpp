#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/tflite_attrs.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/combo_box2.hpp"
#include "gui/dialogs/multiple_selector.hpp"
#include "gettext.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(aismart, tflite_attrs)

ttflite_attrs::ttflite_attrs(tnetwork_item& item)
	: item_(item)
{
}

void ttflite_attrs::pre_show()
{
	window_->set_label("misc/white-background.png");

	tbutton* button = find_widget<tbutton>(window_, "title", false, true);
	button->set_label(item_.name);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&ttflite_attrs::click_name
			, this));

	find_widget<tbutton>(window_, "cancel", false).set_icon("misc/back.png");

	button = find_widget<tbutton>(window_, "state", false, true);
	button->set_icon("misc/state.png");
	std::string state_label = treadme::state_name(item_.state);
	if (state_label.empty()) {
		state_label = _("Not set");
	}
	button->set_canvas_variable("value", variant(state_label));
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&ttflite_attrs::click_state
			, this));

	button = find_widget<tbutton>(window_, "keywords", false, true);
	std::string keywords_lable = treadme::keywords_name(item_.keywords);
	if (keywords_lable.empty()) {
		keywords_lable = _("Not set");
	}
	button->set_label(keywords_lable);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
			&ttflite_attrs::click_keys
			, this));

	tscroll_text_box* description = find_widget<tscroll_text_box>(window_, "description", false, true);
	description->set_placeholder(_("Model's description"));
	description->set_label(item_.description);
}

void ttflite_attrs::post_show()
{
	tscroll_text_box* description = find_widget<tscroll_text_box>(window_, "description", false, true);
	item_.description = description->label();
}

bool ttflite_attrs::did_verify_name(const std::string& label)
{
	const int min_tflite_name_size = 6;
	const int max_tflite_name_size = 24;
	if (!utils::isvalid_id(label, true, min_tflite_name_size, max_tflite_name_size)) {
		return false;
	}

	if (label == item_.name) {
		return false;
	}

	return true;
}

void ttflite_attrs::click_name()
{
	gui2::tedit_box::tparam param(null_str, null_str, _("New name"), item_.name, null_str, 30);
	param.text_changed = boost::bind(&ttflite_attrs::did_verify_name, this, _1);
	gui2::tedit_box dlg(param);
	dlg.show();

	int res = dlg.get_retval();
	if (res != gui2::twindow::OK) {
		return;
	}
	
	item_.name = param.result;

	tbutton& widget = find_widget<tbutton>(window_, "title", false);
	widget.set_label(item_.name);
}

void ttflite_attrs::click_state()
{
	std::vector<std::string> items;
	std::vector<int> values;
	
	int initial = nposm;
	for (std::map<std::string, std::string>::const_iterator it = treadme::support_states.begin(); it != treadme::support_states.end(); ++ it) {		
		items.push_back(it->second);
		if (item_.state == it->first) {
			initial = items.size() - 1;
		}
	}

	gui2::tcombo_box2 dlg(_("Select state"), items, initial);
	dlg.show();
	if (dlg.get_retval() != twindow::OK) {
		return;
	}

	std::map<std::string, std::string>::const_iterator find_it = treadme::support_states.begin();
	std::advance(find_it, dlg.cursel());
	item_.state = find_it->first;

	
	tbutton* sim = find_widget<tbutton>(window_, "state", false, true);
	sim->set_canvas_variable("value", variant(find_it->second));
}

bool ttflite_attrs::did_keyword_selector_changed(ttoggle_button& widget, const int at, const std::vector<bool>& states)
{
	int selected = 0;
	for (std::vector<bool>::const_iterator it = states.begin(); it != states.end(); ++ it) {
		if (*it) {
			selected ++;
		}
	}

	const int max_keywords = 3;
	return selected > 0 && selected <= max_keywords;
}

void ttflite_attrs::click_keys()
{
	std::vector<std::string> items;
	int at = 0;
	int value = nposm;
	std::stringstream ss;

	std::set<int> initial_selected;
	for (std::map<std::string, std::string>::const_iterator it = treadme::support_keywords.begin(); it != treadme::support_keywords.end(); ++ it) {		
		items.push_back(it->second);
		if (std::find(item_.keywords.begin(), item_.keywords.end(), it->first) != item_.keywords.end()) {
			initial_selected.insert(items.size() - 1);
		}
	}

	gui2::tmultiple_selector dlg(_("Select keyword"), items, initial_selected);
	dlg.set_did_selector_changed(boost::bind(&ttflite_attrs::did_keyword_selector_changed, this, _1, _2, _3));
	dlg.show();
	if (dlg.get_retval() != twindow::OK) {
		return;
	}

	item_.keywords.clear();
	const std::set<int>& selected = dlg.selected();
	for (std::set<int>::const_iterator it = selected.begin(); it != selected.end(); ++ it) {
		int at = *it;
		std::map<std::string, std::string>::const_iterator find_it = treadme::support_keywords.begin();
		std::advance(find_it, at);
		item_.keywords.push_back(find_it->first);
	}

	tbutton* button = find_widget<tbutton>(window_, "keywords", false, true);
	std::string keywords_label = treadme::keywords_name(item_.keywords);
	if (keywords_label.empty()) {
		keywords_label = _("Not set");
	}
	button->set_label(keywords_label);
}

} // namespace gui2

