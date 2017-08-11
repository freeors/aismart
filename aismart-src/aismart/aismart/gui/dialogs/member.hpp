#ifndef GUI_DIALOGS_MEMBER_HPP_INCLUDED
#define GUI_DIALOGS_MEMBER_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

namespace gui2 {

class tmember: public tdialog
{
public:
	explicit tmember(int vip_op);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_select();
	void click_commit();

private:
	const int vip_op_;

	surface screenshot_;
};

} // namespace gui2

#endif

