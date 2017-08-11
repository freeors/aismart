#define GETTEXT_DOMAIN "aismart-lib"

#include "gui/dialogs/tflite_schema.hpp"

#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/tree.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/stack.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gettext.hpp"
#include "font.hpp"
#include "game_config.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_DIALOG(aismart, tflite_schema)

ttflite_schema::ttflite_schema(const std::string& file)
	: file_(file)
	, body_(nullptr)
{
	tflite::load_model(file, session_, game_config::showcase_sha256);
	VALIDATE(session_.valid(), null_str);
}

void ttflite_schema::pre_show()
{
	window_->set_label("misc/white-background.png");

	body_ = find_widget<tstack>(window_, "body_stack", false, true);

	find_widget<tlabel>(window_, "title", false).set_label(session_.key);
	find_widget<tcontrol>(window_, "ok", false).set_icon("misc/back.png");

	tbutton* button = find_widget<tbutton>(window_, "home", false, true);
	connect_signal_mouse_left_click(
			*button
			, boost::bind(
				&ttflite_schema::click_home
				, this));
	button->set_visible(twidget::INVISIBLE);

	pre_home();
}

void ttflite_schema::post_show()
{
}

void ttflite_schema::click_home()
{
	tstack& stack = find_widget<tstack>(window_, "body_stack", false);
	stack.set_radio_layer(HOME_LAYER);

	tbutton& ok = find_widget<tbutton>(window_, "ok", false);
	VALIDATE(ok.get_visible() == twidget::INVISIBLE, null_str);
	ok.set_visible(twidget::VISIBLE);

	tbutton& home = find_widget<tbutton>(window_, "home", false);
	VALIDATE(home.get_visible() == twidget::VISIBLE, null_str);
	home.set_visible(twidget::INVISIBLE);

	find_widget<tlabel>(window_, "title", false).set_label(session_.key);
}

const char* kEmptyTensorName = "";
// namespace {
// copy from <lite>/model.cc
std::vector<int> FlatBufferIntArrayToVector2(const flatbuffers::Vector<int32_t>* flat_array) {
  std::vector<int> ret(flat_array->Length());
  for (int i = 0; i < (int)flat_array->Length(); i++) {
    ret[i] = flat_array->Get(i);
  }
  return ret;
}
// }

std::string node2_2_str(tflite::Interpreter& interpreter, const std::pair<TfLiteNode, TfLiteRegistration>& node2)
{
	const TfLiteNode& node = node2.first;
	const TfLiteRegistration& registration = node2.second;

	const char* builtin_code_name = tflite::EnumNameBuiltinOperator((tflite::BuiltinOperator)(registration.builtin_code));
	VALIDATE(builtin_code_name, null_str);

	std::stringstream ss;
	for (int at2 = 0; at2 < node.outputs->size; at2 ++) {
		TfLiteTensor* output = interpreter.tensor(node.outputs->data[at2]);
		ss << (output->name? output->name: "nil") << " ";
	}

	ss << ht::generate_format(utils::lowercase(builtin_code_name), 0xffff3b30);

	ss << "(";
	for (int at2 = 0; at2 < node.inputs->size; at2 ++) {
		TfLiteTensor* input = interpreter.tensor(node.inputs->data[at2]);
		if (at2 != 0) {
			ss << ", ";
		}
		ss << (input->name? input->name: "nil") << " ";
	}
	ss << ")";
	return ss.str();
}

//
// home layer
//
void ttflite_schema::pre_home()
{
	tgrid* home_layer = body_->layer(HOME_LAYER);

	tflite::Interpreter& interpreter = *session_.interpreter.get();
	const tflite::Model& model = *session_.model->GetModel();
	std::stringstream ss;
	std::map<std::string, std::string> data;
	ttree& tree = find_widget<ttree>(body_, "architecture", false);

	// builtin operator
	int builtin_operators = 0;
	for (int code = tflite::BuiltinOperator_MIN; code <= tflite::BuiltinOperator_MAX; code ++) {
		const char* builtin_code_name = tflite::EnumNameBuiltinOperator((tflite::BuiltinOperator)(code));
		if (!builtin_code_name || builtin_code_name[0] == '\0') {
			continue;
		}
		builtin_operators ++;
	}
	ss.str("");
	ss << _("Builtin Operator") << ", count: " << builtin_operators;
	data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
	ttree_node* tmp = &tree.insert_node("default", data);
	tmp->set_cookie(BUILTIN_OPERATORS);

	//
	// schema
	//
	data["text"] = "schema";
	ttree_node* node = &tree.insert_node("default", data);
	node->set_child_icon("text", "misc/tensorflow.png");
	{
		auto opcodes = model.operator_codes();
		ss.str("");
		ss << "operator_codes, count: " << opcodes->Length();
	}
	data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
	tmp = &node->insert_node("default", data);
	tmp->set_cookie(OPERATOR_CODES);
	// subgraphs
	ss.str("");
	ss << "subgraphs, count: " << (uint32_t)session_.model->GetModel()->subgraphs()->Length();
	data["text"] = ss.str();
	tmp = &node->insert_node("default", data);
	tmp->set_child_icon("text", "misc/graph.png");
	const tflite::SubGraph* subgraph = (*session_.model->GetModel()->subgraphs())[0];
	{
		// tensors
		ss.str("");
		ss << "tensors, count: " << (uint32_t)subgraph->tensors()->Length();
		data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
		ttree_node* tmp2 = &tmp->insert_node("default", data);
		tmp2->set_child_icon("text", "misc/variable.png");
		tmp2->set_cookie(SUBGRAPH_TENSORS);
		// inputs
		ss.str("");
		ss << "inputs: ";
		std::vector<int> inputs = FlatBufferIntArrayToVector2(subgraph->inputs());
		for (std::vector<int>::const_iterator it = inputs.begin(); it != inputs.end(); ++ it) {
			if (it != inputs.begin()) {
				ss << ", ";
			}
			ss << "#" << *it;
		}
		data["text"] = ss.str();
		tmp->insert_node("default", data);
		// outputs
		ss.str("");
		ss << "outputs: ";
		std::vector<int> outputs = FlatBufferIntArrayToVector2(subgraph->outputs());
		for (std::vector<int>::const_iterator it = outputs.begin(); it != outputs.end(); ++ it) {
			if (it != outputs.begin()) {
				ss << ", ";
			}
			ss << "#" << *it;
		}
		data["text"] = ss.str();
		tmp->insert_node("default", data);
		// operators
		ss.str("");
		ss << "operators: count: " << (uint32_t)subgraph->operators()->Length();
		data["text"] = ss.str();
		tmp2 = &tmp->insert_node("default", data);
		tmp2->set_child_icon("text", "misc/code.png");
		// name
		ss.str("");
		const flatbuffers::String* str = subgraph->name();
		ss << "name: " << (str? str->str(): "<nil>");
		data["text"] = ss.str();
		tmp->insert_node("default", data);
	}
	// description
	ss.str("");
	ss << "description: " << session_.model->GetModel()->description()->str();
	data["text"] = ss.str();
	tmp = &node->insert_node("default", data);

	// buffers
	ss.str("");
	ss << "buffers, count: " << (uint32_t)model.buffers()->Length();
	data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
	tmp = &node->insert_node("default", data);
	tmp->set_cookie(BUFFERS);

	//
	// tflite::Interpreter
	//
	data["text"] = "tflite::Interpreter";
	node = &tree.insert_node("default", data);
	node->set_child_icon("text", "misc/list.png");
	// tensors
	ss.str("");
	ss << "context_.tensors, count: " << interpreter.tensors_size() << ", ";
	ss << (interpreter.tensors_size() - subgraph->tensors()->Length()) << " bonus tensors";
	data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
	tmp = &node->insert_node("default", data);
	tmp->set_child_icon("text", "misc/variable.png");
	tmp->set_cookie(TENSORS);

	const std::vector<int>& inputs = interpreter.inputs();
	for (std::vector<int>::const_iterator it = inputs.begin(); it != inputs.end(); ++ it) {
		TfLiteTensor* tensor = interpreter.tensor(*it);
		VALIDATE(tensor->name, null_str);

		ss.str("");
		ss << "input, #" << *it << ", " << tensor->name;
		data["text"] = ss.str();
		tmp->insert_node("default", data);
	}
	const std::vector<int>& outputs = interpreter.outputs();
	for (std::vector<int>::const_iterator it = outputs.begin(); it != outputs.end(); ++ it) {
		TfLiteTensor* tensor = interpreter.tensor(*it);
		VALIDATE(tensor->name, null_str);

		ss.str("");
		ss << "output, #" << *it << ", " << tensor->name;
		data["text"] = ss.str();
		tmp->insert_node("default", data);
	}
	// invoke
	ss.str("");
	ss << "Invoke(interpreter.nodes_and_registration_)" << ", count: " << interpreter.nodes_size();
	data["text"] = ht::generate_format(ss.str(), 0xff0000ff);
	tmp = &node->insert_node("default", data);
	tmp->set_child_icon("text", "misc/code.png");
	tmp->set_cookie(INVOKE);
	if (interpreter.nodes_size() > 0) {
		std::set<int> indexs;
		indexs.insert(0);
		indexs.insert(interpreter.nodes_size() - 1);
		for (std::set<int>::const_iterator it = indexs.begin(); it != indexs.end(); ++ it) {
			const int at = *it;
			const std::pair<TfLiteNode, TfLiteRegistration>* node2 = interpreter.node_and_registration(at);

			ss.str("");
			ss << "#" << at << ", " << node2_2_str(interpreter, *node2);

			data["text"] = font::make_text_ellipsis(ss.str(), font::SIZE_DEFAULT, settings::screen_width);

			tmp->insert_node("default", data);
		}
	}

	tree.get_root_node().unfold_children();
	tree.enable_select(false);
	tree.set_did_node_changed(boost::bind(&ttflite_schema::did_architecture_clicked, this, _2));
}

void ttflite_schema::did_architecture_clicked(ttree_node& node)
{
	int cookie = static_cast<int>(node.cookie());
	if (cookie == nposm) {
		return;
	}

	tbutton& ok = find_widget<tbutton>(window_, "ok", false);
	VALIDATE(ok.get_visible() != twidget::INVISIBLE, null_str);

	tbutton& home = find_widget<tbutton>(window_, "home", false);
	VALIDATE(home.get_visible() != twidget::VISIBLE, null_str);
	tlabel& title = find_widget<tlabel>(window_, "title", false);

	tstack& stack = find_widget<tstack>(window_, "body_stack", false);
	if (cookie == OPERATOR_CODES) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label("schema.operator_codes");
		stack.set_radio_layer(TENSORS_LAYER);
		fill_operator_codes();

	} else if (cookie == SUBGRAPH_TENSORS) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label("schema.subgraph[0].tensors");
		stack.set_radio_layer(TENSORS_LAYER);
		fill_subgraph_tensors();

	} else if (cookie == BUFFERS) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label("schema.buffers");
		stack.set_radio_layer(TENSORS_LAYER);
		fill_buffers();

	} else if (cookie == BUILTIN_OPERATORS) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label(_("Builtin Operator"));
		stack.set_radio_layer(TENSORS_LAYER);
		fill_builtin_operators();

	} else if (cookie == TENSORS) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label("context_.tensors");
		stack.set_radio_layer(TENSORS_LAYER);
		fill_tensors();

	} else if (cookie == INVOKE) {
		ok.set_visible(twidget::INVISIBLE);
		home.set_visible(twidget::VISIBLE);
		title.set_label("Invoke");
		stack.set_radio_layer(TENSORS_LAYER);
		fill_invoke();
	}
}

void ttflite_schema::fill_operator_codes()
{
	tgrid* builtin_operators_layer = body_->layer(TENSORS_LAYER);

	const tflite::Model& model = *session_.model->GetModel();
	auto opcodes = model.operator_codes();

	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(builtin_operators_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	int at = 0;
	for (const tflite::OperatorCode* opcode : *opcodes) {
		ss.str("");
		auto code = opcode->builtin_code();
		if (code != tflite::BuiltinOperator_CUSTOM) {
			const char* builtin_code_name = tflite::EnumNameBuiltinOperator((tflite::BuiltinOperator)(code));
			if (builtin_code_name && builtin_code_name[0] != '\0') {
				ss << builtin_code_name;
			}
		} else {
			ss << "<custom>";
		}

		data["index"] = str_cast(at ++);
		data["label"] = utils::lowercase(ss.str());
		list.insert_row(data);
	}
}

//
// subgraph tensor layer
//
void ttflite_schema::fill_subgraph_tensors()
{
	// A little helper to get the names of inputs and outputs. Note that they
	// must outlive the interpreter.
	auto get_name = [](const tflite::Tensor* t) -> const char* {
		auto name = t->name();
		if (name) return name->c_str();
		return kEmptyTensorName;
	};

	tgrid* subgraph_tensors_layer = body_->layer(TENSORS_LAYER);

	const tflite::SubGraph* subgraph = (*session_.model->GetModel()->subgraphs())[0];
	auto tensors = subgraph->tensors();

	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(subgraph_tensors_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	for (int at = 0; at < (int)tensors->Length(); ++ at) {
		const auto* tensor = tensors->Get(at);
		std::vector<int> dims = FlatBufferIntArrayToVector2(tensor->shape());

		data["index"] = str_cast(at);
		ss.str("");
		// shapce
		ss << "[";
		for (std::vector<int>::const_iterator it = dims.begin(); it != dims.end(); ++ it) {
			if (it != dims.begin()) {
				ss << ", ";
			}
			ss << *it;
		}
		ss << "], ";
		// type
		ss << tflite::EnumNameTensorType(tensor->type()) << ", ";
		// buffer
		ss << "data at buffer#" << tensor->buffer() << "\n";

		// name
		const char* name = get_name(tensor);
		ss << (name? name: null_str);

		data["label"] = ss.str();
		list.insert_row(data);
	}
}

void ttflite_schema::fill_buffers()
{
	tgrid* builtin_operators_layer = body_->layer(TENSORS_LAYER);
	const tflite::Model& model = *session_.model->GetModel();
	auto* buffers = model.buffers();

	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(builtin_operators_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	for (int at = 0; at < (int)buffers->size(); at ++) {
		ss.str("");
		if (auto* buffer = (*buffers)[at]) {
			if (auto* array = buffer->data()) {
				// if (size_t size = array->size()) {
				if (uint32_t size = array->size()) {
					ss << "size: " << size;
					// *buffer_data = reinterpret_cast<const char*>(array->data());
				}
			}
		}
		if (ss.str().empty()) {
			ss << "<nil>";
		}

		data["index"] = str_cast(at);
		data["label"] = ss.str();
		list.insert_row(data);
	}
}

void ttflite_schema::fill_builtin_operators()
{
	tgrid* builtin_operators_layer = body_->layer(TENSORS_LAYER);

	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(builtin_operators_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	int at = 0;
	for (int code = tflite::BuiltinOperator_MIN; code <= tflite::BuiltinOperator_MAX; code ++) {
		const char* builtin_code_name = tflite::EnumNameBuiltinOperator((tflite::BuiltinOperator)(code));
		if (!builtin_code_name || builtin_code_name[0] == '\0') {
			continue;
		}

		data["index"] = str_cast(at ++);
		data["label"] = utils::lowercase(builtin_code_name);
		list.insert_row(data);
	}
}

//
// tensor layer
//
inline const char **EnumNamesTfLiteType() {
  static const char *names[] = {"NoType", "Float32", "Int32", "UInt8",
                                "Int64",   "String",  nullptr};
  return names;
}

inline const char *EnumNameTfLiteType(TfLiteType e) {
  const size_t index = static_cast<int>(e);
  return EnumNamesTfLiteType()[index];
}
void ttflite_schema::fill_tensors()
{
	tgrid* tensors_layer = body_->layer(TENSORS_LAYER);

	std::stringstream ss;
	tflite::Interpreter& interpreter = *session_.interpreter.get();
	const int tensors_size = interpreter.tensors_size();
	const std::vector<int>& inputs = interpreter.inputs();
	const std::vector<int>& outputs = interpreter.outputs();

	tlistbox& list = find_widget<tlistbox>(tensors_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	for (int at = 0; at < tensors_size; at ++) {
		TfLiteTensor* tensor = interpreter.tensor(at);
		data["index"] = str_cast(at);

		ss.str("");
		// shape
		ss << "[";
		for (int at2 = 0; tensor->dims && at2 < tensor->dims->size; at2 ++) {
			if (at2 != 0) {
				ss << ", ";
			}
			ss << tensor->dims->data[at2];
		}
		ss << "], ";
		// type
		ss << EnumNameTfLiteType(tensor->type) << ", ";

		// buffer
		if (tensor->bytes && tensor->data.raw) {
			VALIDATE(tensor->data.raw, null_str);
			ss << tensor->bytes << " Bytes\n";
		} else {
			ss << " 0 Bytes\n";
		}
		
		// name
		ss << (tensor->name? tensor->name: null_str);
		data["label"] = ss.str();
		ttoggle_panel& row = list.insert_row(data);

		std::string png;
		tpanel& control = *dynamic_cast<tpanel*>(row.find("panel", false));

		if (std::find(inputs.begin(), inputs.end(), at) != inputs.end()) {
			png = "misc/translucent54-background.png";
		} else if (std::find(outputs.begin(), outputs.end(), at) != outputs.end()) {
			png = "misc/translucent54-background.png";
		}
		if (!png.empty()) {
			control.set_label(png);
		}
	}
}

//
// invoke layer
//
void ttflite_schema::fill_invoke()
{
	tgrid* tensors_layer = body_->layer(TENSORS_LAYER);

	tflite::Interpreter& interpreter = *session_.interpreter.get();
	const int nodes_size = interpreter.nodes_size();

	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(tensors_layer, "tensors", false);
	list.clear();
	list.enable_select(false);
	std::map<std::string, std::string> data;
	for (int at = 0; at < nodes_size; at ++) {
		const std::pair<TfLiteNode, TfLiteRegistration>* node2 = interpreter.node_and_registration(at);

		data["index"] = str_cast(at);
		data["label"] = node2_2_str(interpreter, *node2);
		list.insert_row(data);
	}
}

} // namespace gui2

