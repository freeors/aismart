#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/member.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/photo_selector.hpp"
#include "gui/dialogs/browse.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "net.hpp"
#include "filesystem.hpp"

#include "SDL_image.h"
#include <boost/bind.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace gui2 {

REGISTER_DIALOG(aismart, member)

tmember::tmember(int vip_op)
	: vip_op_(vip_op)
{
	VALIDATE(current_user.valid() && vip_op_ != nposm, null_str);
}

void tmember::pre_show()
{
	window_->set_label("misc/white-background.png");

	utils::string_map symbols;
	std::string title, steps_section, step1, step2, step3, step4;
	std::string money;
	if (vip_op_ == vip_op_join) {
		title = _("Join VIP");
		steps_section = _("The steps of be a VIP");
		symbols["money"] = _("400 RMB(VIP1) or 800 RBM(VIP2)");

	} else if (vip_op_ == vip_op_renew) {
		title = _("Renew VIP");
		steps_section = _("The steps of renew VIP");
		symbols["money"] = _("400 RMB(VIP1) or 800 RBM(VIP2)");

	} else if (vip_op_ == vip_op_update) {
		title = _("Update VIP");
		steps_section = _("The steps of update VIP");
		const int total = 400;
		const int least = total / 12 + 1;
		time_t different = (current_user.vip_ts + 365 * 24 * 3600 - time(nullptr)) / (24 * 3600);
		int amount = total * different / 365;
		if (amount < least) {
			amount = least;
		}
		utils::string_map symbols2;
		symbols2["amount"] = str_cast(amount);

		symbols["money"] = vgettext2("$amount RMB", symbols2);
	}

	step1 = vgettext2("Run Alipay, pay $money to the right side of account.", symbols);
	step2 = _("Cut a screenshot that contains 'Bill details' of the transaction record. There is at least the payment time, amount and the payer.");
	step3 = _("Upload screenshot to AI Smart.");

	find_widget<tlabel>(window_, "title", false).set_label(title);
	find_widget<tlabel>(window_, "steps_section", false).set_label(steps_section);
	find_widget<tlabel>(window_, "step1", false).set_label(step1);
	find_widget<tlabel>(window_, "step2", false).set_label(step2);
	find_widget<tlabel>(window_, "step3", false).set_label(step3);

	find_widget<tbutton>(window_, "cancel", false).set_icon("misc/back.png");

	tlistbox& capability = find_widget<tlistbox>(window_, "capability", false);
	capability.enable_select(false);

	std::map<std::string, std::string> data;

	// net disk
	data["parameter"] = _("Net disk");
	data["normal"] = "10 MB";
	data["vip1"] = "40 MB";
	data["vip2"] = "100 MB";
	capability.insert_row(data);

	// annual fee
	data["parameter"] = _("Annual fee");
	data["normal"] = "0";
	data["vip1"] = "400 RMB";
	data["vip2"] = "800 RMB";
	capability.insert_row(data);

	tscroll_text_box& annotation = find_widget<tscroll_text_box>(window_, "annotation", false);
	annotation.set_placeholder(_("Annotation"));

	tbutton* button = find_widget<tbutton>(window_, "select", false, true);
	connect_signal_mouse_left_click(
			*button
		, boost::bind(
			&tmember::click_select
			, this));

	button = find_widget<tbutton>(window_, "commit", false, true);
	connect_signal_mouse_left_click(
			*button
		, boost::bind(
			&tmember::click_commit
			, this));
	button->set_active(false);
}

void tmember::post_show()
{
}

void tmember::click_select()
{
	std::string dir = game_config::path + "/" + game_config::app_dir + "/images/misc";

	if (game_config::mobile) {
		gui2::tphoto_selector dlg(_("Select pay screenshot"), dir);
		dlg.show();
		if (dlg.get_retval() != twindow::OK) {
			return;
		}
		screenshot_ = dlg.selected_surf();

	} else {
		std::string initial = game_config::preferences_dir;

		gui2::tbrowse::tparam param(gui2::tbrowse::TYPE_FILE, true, initial, _("Choose a screenshot to Load"));
		param.extra = gui2::tbrowse::tentry(game_config::path + "/data/gui/default/window", _("gui/window"), "misc/dir-res.png");

		gui2::tbrowse dlg(param);
		dlg.show();
		int res = dlg.get_retval();
		if (res != gui2::twindow::OK) {
			return;
		}
		surface surf = image::get_image(param.result);
		if (!surf.get()) {
			return;
		}
		screenshot_ = surf;
	}

	tcontrol* image = find_widget<tcontrol>(window_, "screenshot", false, true);

	std::vector<gui2::tformula_blit> blits;
	blits.push_back(gui2::tformula_blit(screenshot_, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_LT, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER));
	image->set_blits(blits);

	find_widget<tbutton>(window_, "commit", false, true)->set_active(true);

	// imwrite(screenshot_, "1.png");
}

void tmember::click_commit()
{
	VALIDATE(screenshot_, null_str);

	unsigned char* data = nullptr;
	int len = IMG_SavePNG_RW(screenshot_, (SDL_RWops*)(&data), 0x5a5a);
/*
	{
		tfile file(game_config::preferences_dir + "/2.png", GENERIC_WRITE, CREATE_ALWAYS);
		posix_fwrite(file.fp, data, len);
		int ii = 0;
	}
*/
	tscroll_text_box& annotation = find_widget<tscroll_text_box>(window_, "annotation", false);
	const std::string& text = annotation.label();

	{
		tdisable_idle_lock lock;
		bool ret = run_with_progress(_("Uploading"), boost::bind(&net::do_vip, current_user.sessionid, vip_op_, text, (const char*)data, len), true, 0, false);
		SDL_free(data);
		if (!ret) {
			return;
		}
	}

	window_->set_retval(twindow::OK);
}

} // namespace gui2

