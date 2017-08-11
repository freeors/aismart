#ifndef GUI_DIALOGS_TFLITE_SCHEMA_HPP_INCLUDED
#define GUI_DIALOGS_TFLITE_SCHEMA_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"
#include "tflite.hpp"

namespace gui2 {

class ttree_node;
class tstack;

class ttflite_schema: public tdialog
{
public:
	explicit ttflite_schema(const std::string& tffile);

private:
	/** Inherited from tdialog. */
	void pre_show() override;

	/** Inherited from tdialog. */
	void post_show() override;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	void click_home();
	enum {HOME_LAYER, TENSORS_LAYER};

	// home
	enum {OPERATOR_CODES, SUBGRAPH_TENSORS, BUFFERS, BUILTIN_OPERATORS, TENSORS, INVOKE};
	void pre_home();
	void did_architecture_clicked(ttree_node& node);

	// subgraph tensors
	void fill_operator_codes();
	void fill_subgraph_tensors();
	void fill_buffers();
	void fill_builtin_operators();

	// tensors
	void fill_tensors();

	// invoke
	void fill_invoke();

private:
	const std::string& file_;
	tflite::tsession session_;

	tstack* body_;
};

} // namespace gui2

#endif

