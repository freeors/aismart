#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/about.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/window.hpp"
#include "gettext.hpp"
#include "rose_config.hpp"

namespace gui2 {

REGISTER_DIALOG(aismart, about)

tabout::tabout()
{
}

void tabout::pre_show()
{
	window_->set_label("misc/white-background.png");

	find_widget<tcontrol>(window_, "cancel", false).set_icon("misc/back.png");

	tcontrol* widget = find_widget<tcontrol>(window_, "version", false, true);
	widget->set_label(game_config::get_app_msgstr(game_config::app) + "   " + game_config::version);

	widget = find_widget<tcontrol>(window_, "help", false, true);

	surface surf = image::get_image("misc/guide.png");
	widget->set_blits(tformula_blit(surf, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER, tformula_blit::SURF_RATIO_CENTER));
}

void tabout::post_show()
{
}

} // namespace gui2

