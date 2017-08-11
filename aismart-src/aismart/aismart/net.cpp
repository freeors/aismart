#define GETTEXT_DOMAIN "aismart-lib"

#include "net.hpp"
#include "filesystem.hpp"
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "gui/dialogs/message.hpp"
#include "json/json.h"
#include "game_config.hpp"
#include "formula_string_utils.hpp"

#include <iomanip>

#define ERRCODE_MOBILE_REGISTED	-2
#define ERRCODE_NO_MOBILE		-3
#define ERRCODE_PASSWORD_ERROR	-7
#define ERRCODE_REMOTE_LOGIN	-9
#define ERRCODE_LOW_VERSION		-14


const int vericode_reserve_secs = 60;
const int max_vericode_size = 6;
const std::string showcase_identifier = "showcase";

const tuser null_user2;
tuser current_user;

bool tuser::valid() const 
{ 
	bool _valid = !sessionid.empty();
	if (_valid) {
		VALIDATE(!mobile.empty(), null_str);
	}
	return _valid;
}

int calculate_vip_op()
{
	if (!current_user.valid()) {
		return nposm;
	}
	if (current_user.vip) {
		time_t now = time(nullptr);
		int64_t remaindr_second = current_user.vip_ts + 365 * 24 * 3600 - now;
		const int64_t min_expire_warn = 30 * 24 * 3600;
		if (remaindr_second > 0 && remaindr_second <= min_expire_warn) {
			return vip_op_renew;
		} else if (current_user.vip == 1) {
			return vip_op_update;
		} else {
			VALIDATE(current_user.vip == 2, null_str);
			return nposm;
		}
	}
	return vip_op_join;
}

bool check_vericode_size(const int size)
{
	return size == 4 || size == 6;
}

bool network_require_login()
{
	if (!current_user.valid()) {
		gui2::show_message(null_str, _("You are not logged in"));
		return false;
	}
	return true;
}

namespace net {

static std::string generic_handle_response(const int code, Json::Value& json_object, bool& handled)
{
	std::stringstream err;
	utils::string_map symbols;
	
	handled = false;
	if (code == ERRCODE_MOBILE_REGISTED) {
		err << _("Mobile is existed");

	} else if (code == ERRCODE_NO_MOBILE) {
		err << _("Mobile isn't existed");

	} else if (code == ERRCODE_PASSWORD_ERROR) {
		err << _("Password error");

	} else if (code == ERRCODE_REMOTE_LOGIN) {
		Json::Value& timestamp = json_object["timestamp"];

		symbols["time"] = format_time_date(timestamp.asInt64());
		err << vgettext2("Your account is logined at $time in different places. If it is not your operation, your password has been leaked. Please as soon as possible to modify the password.", symbols);
		current_user.sessionid.clear();

		handled = true;

	} else if (code == ERRCODE_LOW_VERSION) {
		err << _("The version is too low, please upgrade AI Smart.");
		handled = true;
	}

	return err.str();
}

static const char* vip_op_type[] = {
	"join",
	"renew",
	"update"
};
static int nb_vip_op_type = sizeof(vip_op_type) / sizeof(vip_op_type[0]);

static std::string vip_op_int_2_json(int vip_op)
{
	VALIDATE(vip_op >= 0 && vip_op < nb_vip_op_type, null_str);
	return vip_op_type[vip_op];
}

static int vip_op_json_2_int(const std::string& json)
{
	if (json.empty()) {
		return nposm;
	}

	const char* json_c_str = json.c_str();
	for (int i = 0; i < nb_vip_op_type; i ++) {
		if (SDL_strcmp(json_c_str, vip_op_type[i]) == 0) {
			return i;
		}
	}

	VALIDATE(false, std::string("In login response, unknown vip_op: ") + json);
	return nposm;
}

std::string form_url(const std::string& category, const std::string& task)
{
	std::stringstream url;

	url << "https://" << game_config::bbs_server.host << game_config::bbs_server.url << category << "/";
	url << task;

	return url.str();
}

bool did_pre_login(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["mobile"] = mobile;
	json_root["password"] = password;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_login(const net::RoseDelegate& delegate, int status, const std::string& mobile, bool quiet, tuser& user)
{
	std::stringstream err;
	user.mobile.clear();
	user.sessionid.clear();

	if (status != net::OK) {
		if (!quiet) {
			err << net::err_2_description(status);
			gui2::show_message(null_str, err.str());
		}
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
			
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		Json::Value& results = json_object["results"];
		if (results.isObject()) {
			Json::Value& nick = results["nickname"];
			Json::Value& sessionid = results["sessionid"];
			Json::Value& vip = results["vip"];
			Json::Value& vip_ts = results["vip_ts"];
			Json::Value& vip_op = results["vip_op"];

			user.mobile = mobile;
			user.nick = nick.asString();
			user.sessionid = sessionid.asString();
			user.vip = vip.asInt();
			user.vip_op = vip_op_json_2_int(vip_op.asString());
			user.vip_ts = vip_ts.asInt64();
		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !user.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		if (!quiet) {
			gui2::show_message(null_str, err.str());
		}
		return false;
	}

	return true;
}

bool do_login(const std::string& mobile, const std::string& password, bool quiet, tuser& user)
{
	net::thttp_agent agent(form_url("aismart/api", "login.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_login, _2, _3, mobile, password);
	agent.did_post = boost::bind(&did_post_login, _2, _3, mobile, quiet, boost::ref(user));

	return net::handle_http_request(agent);
}

bool did_pre_logout(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid)
{
	VALIDATE(!sessionid.empty(), null_str);
	
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_logout(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_logout(const std::string& sessionid)
{
	net::thttp_agent agent(form_url("aismart/api", "logout.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_logout, _2, _3, sessionid);
	agent.did_post = boost::bind(&did_post_logout, _2, _3);

	return net::handle_http_request(agent);
}

static bool allowed_attribute_key(const std::string& key)
{
	std::set<std::string> keys;
	keys.insert("nickname");

	return keys.count(key) > 0;
}

bool did_pre_updateattrs(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::map<std::string, std::string>& attrs)
{
	VALIDATE(!sessionid.empty() && !attrs.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	for (std::map<std::string, std::string>::const_iterator it = attrs.begin(); it != attrs.end(); ++ it) {
		const std::string& key = it->first;
		const std::string& val = it->second;

		VALIDATE(allowed_attribute_key(key), null_str);
		json_root[key] = val;
	}
	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_updateattrs(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_updateattrs(const std::string& sessionid, const std::map<std::string, std::string>& attrs)
{
	net::thttp_agent agent(form_url("aismart/api", "updateattrs.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_updateattrs, _2, _3, sessionid, boost::ref(attrs));
	agent.did_post = boost::bind(&did_post_updateattrs, _2, _3);

	return net::handle_http_request(agent);
}

bool did_pre_register(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password, const std::string& vericode)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["mobile"] = mobile;
	json_root["password"] = password;
	json_root["vericode"] = vericode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_register(const net::RoseDelegate& delegate, int status, const std::string& mobile, tuser& user)
{
	user.mobile.clear();
	user.sessionid.clear();

	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();
			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		Json::Value& results = json_object["results"];
		if (results.isObject()) {
			Json::Value& nick = results["nickname"];
			Json::Value& sessionid = results["sessionid"];
			Json::Value& vip = results["vip"];
			Json::Value& vip_ts = results["vip_ts"];

			user.mobile = mobile;
			user.nick = nick.asString();
			user.sessionid = sessionid.asString();
			user.vip = vip.asInt();
			user.vip_ts = vip_ts.asInt64();
		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (err.str().empty() && !user.valid()) {
		err << _("Unknown error.");
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_register(const std::string& mobile, const std::string& password, const std::string& vericode, tuser& user)
{
	net::thttp_agent agent(form_url("aismart/api", "register.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_register, _2, _3, mobile, password, vericode);
	agent.did_post = boost::bind(&did_post_register, _2, _3, mobile, boost::ref(user));

	return net::handle_http_request(agent);
}

bool did_pre_erase_user(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& vericode)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["vericode"] = vericode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_erase_user(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_erase_user(const std::string& sessionid, const std::string& vericode)
{
	net::thttp_agent agent(form_url("aismart/api", "eraseuser.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_erase_user, _2, _3, sessionid, vericode);
	agent.did_post = boost::bind(&did_post_erase_user, _2, _3);

	return net::handle_http_request(agent);
}

bool did_pre_resetpwd(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password, const std::string& vericode)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["mobile"] = mobile;
	json_root["password"] = password;
	json_root["vericode"] = vericode;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_resetpwd(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_resetpwd(const std::string& mobile, const std::string& password, const std::string& vericode)
{
	net::thttp_agent agent(form_url("aismart/api", "resetpwd.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_resetpwd, _2, _3, mobile, password, vericode);
	agent.did_post = boost::bind(&did_post_resetpwd, _2, _3);

	return net::handle_http_request(agent);
}

static const char* vericode_type[] = {
	"register",
	"resetpwd",
	"eraseuser"
};
static int nb_vericode_type = sizeof(vericode_type) / sizeof(vericode_type[0]);

static std::string vericode_int_2_json(int type)
{
	VALIDATE(type >= 0 && type < nb_vericode_type, null_str);
	return vericode_type[type];
}

bool did_pre_vericode(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, int type)
{	
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["mobile"] = mobile;
	json_root["type"] = vericode_int_2_json(type);

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_vericode(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}
			
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool do_vericode(const std::string& mobile, int type)
{
	net::thttp_agent agent(form_url("aismart/api", "vericode.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_vericode, _2, _3, mobile, type);
	agent.did_post = boost::bind(&did_post_vericode, _2, _3);

	return net::handle_http_request(agent);
}

bool did_pre_filelist(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	Json::FastWriter writer;

	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_filelist(const net::RoseDelegate& delegate, int status, std::set<tnetwork_item>& items)
{
	items.clear();

	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		const Json::Value& scripts = json_object["scripts"];
		if (scripts.isArray()) {
			for (int at = 0; at < (int)scripts.size(); at ++) {
				const Json::Value& item = scripts[at];
				items.insert(tnetwork_item(item["name"].asString(), item["size"].asInt(), xmit_script));
			}
		}
		const Json::Value& images = json_object["images"];
		if (images.isArray()) {
			for (int at = 0; at < (int)images.size(); at ++) {
				const Json::Value& item = images[at];
				items.insert(tnetwork_item(item["name"].asString(), item["size"].asInt(), xmit_image));
			}
		}
		const Json::Value& models = json_object["models"];
		if (models.isArray()) {
			for (int at = 0; at < (int)models.size(); at ++) {
				const Json::Value& item = models[at];
				tnetwork_item network_item(item["name"].asString(), item["size"].asInt(), xmit_tflite);
				network_item.set_model_special(item["token"].asString(), item["timestamp"].asInt64(), item["state"].asString(), item["keywords"].asString(), item["desc"].asString(), item["showcase"].asBool(), item["follows"].asInt(), item["comments"].asInt());
				items.insert(network_item);
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_filelist(const std::string& sessionid, std::set<tnetwork_item>& items)
{
	net::thttp_agent agent(form_url("aismart/api", "filelist.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_filelist, _2, _3, sessionid);
	agent.did_post = boost::bind(&did_post_filelist, _2, _3, boost::ref(items));

	return net::handle_http_request(agent);
}

static bool allowed_model_attribute_key(const std::string& key)
{
	static std::set<std::string> keys;
	if (keys.empty()) {
		keys.insert("newname");
		keys.insert("keywords");
		keys.insert("state");
		keys.insert("desc");
		keys.insert("showcase");
	}

	return keys.count(key) > 0;
}

bool did_pre_updatemodelattrs(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& name, const std::map<std::string, std::string>& attrs)
{
	VALIDATE(!sessionid.empty() && !attrs.empty(), null_str);

	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["name"] = name;
	for (std::map<std::string, std::string>::const_iterator it = attrs.begin(); it != attrs.end(); ++ it) {
		const std::string& key = it->first;
		const std::string& val = it->second;

		VALIDATE(allowed_model_attribute_key(key), null_str);
		if (key != showcase_identifier) {
			json_root[key] = val;
		} else {
			json_root[key] = utils::to_bool(val);
		}
	}

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_updatemodelattrs(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_updatemodelattrs(const std::string& sessionid, const std::string& name, const std::map<std::string, std::string>& attrs)
{
	net::thttp_agent agent(form_url("aismart/api", "updatemodelattrs.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_updatemodelattrs, _2, _3, sessionid, name, attrs);
	agent.did_post = boost::bind(&did_post_updatemodelattrs, _2, _3);

	return net::handle_http_request(agent);
}

static const char* json_xmit_type[] = {
	"script",
	"tflite",
	"image"
};
static int nb_json_xmit_type = sizeof(json_xmit_type) / sizeof(json_xmit_type[0]);

bool did_pre_upload_file(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files, const int available)
{
	std::stringstream err;
	VALIDATE(!sessionid.empty(), null_str);
	VALIDATE(type >= 0 && type < nb_json_xmit_type && !name.empty() && !files.empty(), null_str);
	
	*body = nullptr;

	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["type"] = json_xmit_type[type];
	json_root["name"] = name;

	int appendix_size = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& file_name = it->second;
		
		tfile file(file_name, GENERIC_READ, OPEN_EXISTING);
		int fsize = posix_fsize(file.fp);
		if (!fsize) {
			continue;
		}
		appendix_size += fsize;
	}
	if (!appendix_size) {
		err << _("Special file is empty, can not upload.");
		gui2::show_message(null_str, err.str());
		return false;
	}
	if (appendix_size > available) {
		err << _("Your available space is not enough, can not upload.");
		gui2::show_message(null_str, err.str());
		return false;
	}

	char* appendix_data = (char*)malloc(appendix_size);
	appendix_size = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& key = it->first;
		const std::string& file_name = it->second;
		std::string offset_key, size_key;
		
		tfile file(file_name, GENERIC_READ, OPEN_EXISTING);
		int fsize = file.read_2_data();
		if (!fsize) {
			continue;
		}

		json_root[key + "_offset"] = appendix_size;
		json_root[key + "_size"] = fsize;
		memcpy(appendix_data + appendix_size, file.data, fsize);
		appendix_size += fsize;
	}

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + appendix_size;

	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	memcpy(buf2 + prefix_size + json_str.size(), appendix_data, appendix_size);

	free(appendix_data);
	appendix_data = nullptr;

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_upload_file(const net::RoseDelegate& delegate, int status, int type, std::string& token)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		if (err.str().empty() && type == xmit_tflite) {
			token = json_object["token"].asString();
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool upload_file(const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files, const int available, std::string& token)
{
	char* body = nullptr;
	net::thttp_agent agent(form_url("aismart/api", "uploadfile.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_upload_file, _1, _2, &body, sessionid, type, name, files, available);
	agent.did_post = boost::bind(&did_post_upload_file, _2, _3, type, boost::ref(token));

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}

bool did_pre_download_file(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files)
{
	VALIDATE(!sessionid.empty(), null_str);
	VALIDATE(!files.empty(), null_str);
	
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["type"] = json_xmit_type[type];
	json_root["name"] = name;

	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& key = it->first;
		json_root[key] = true;
	}

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");

	return true;
}

bool did_post_download_file(const net::RoseDelegate& delegate, int status, const std::vector<std::pair<std::string, std::string> >& files)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		// Json::Value& results = json_object["results"];
		// if (results.isObject()) {
			for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
				const std::string& key = it->first;
				const std::string& file_name = it->second;

				Json::Value& offset = json_object[key + "_offset"];
				Json::Value& size = json_object[key + "_size"];
				int offset_ = offset.asInt();
				int size_ = size.asInt();
				SDL_MakeDirectory(extract_directory(file_name).c_str());
				tfile file(file_name, GENERIC_WRITE, CREATE_ALWAYS);
				posix_fwrite(file.fp, content + prefix_size + json_size + offset_, size_);
			}
		// }

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool download_file(const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files)
{
	net::thttp_agent agent(form_url("aismart/api", "downloadfile.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_download_file, _2, _3, sessionid, type, name, boost::ref(files));
	agent.did_post = boost::bind(&did_post_download_file, _2, _3, boost::ref(files));

	return net::handle_http_request(agent);
}

bool did_pre_deletefile(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, int type, const std::string& name)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["type"] = json_xmit_type[type];
	json_root["name"] = name;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_deletefile(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_deletefile(const std::string& sessionid, int type, const std::string& name)
{
	net::thttp_agent agent(form_url("aismart/api", "deletefile.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_deletefile, _2, _3, sessionid, type, name);
	agent.did_post = boost::bind(&did_post_deletefile, _2, _3);

	return net::handle_http_request(agent);
}

static const char* follow_op_type[] = {
	"follow",
	"unfollow",
	"star"
};
static int nb_follow_op_type = sizeof(follow_op_type) / sizeof(follow_op_type[0]);

bool did_pre_followmodel(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, std::string& token, int op)
{
	VALIDATE(!sessionid.empty() && !token.empty() && op >= 0 && op < nb_follow_op_type, null_str);
	
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["token"] = token;
	json_root["op"] = follow_op_type[op];

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_followmodel(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_followmodel(const std::string& sessionid, std::string& token, int op)
{
	net::thttp_agent agent(form_url("aismart/api", "followmodel.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_followmodel, _2, _3, sessionid, token, op);
	agent.did_post = boost::bind(&did_post_followmodel, _2, _3);

	return net::handle_http_request(agent);
}

bool did_pre_download_model(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& token, const std::vector<std::pair<std::string, std::string> >& files)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["token"] = token;

	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
		const std::string& key = it->first;
		json_root[key] = true;
	}

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_download_model(const net::RoseDelegate& delegate, int status, const std::vector<std::pair<std::string, std::string> >& files)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	
	const char* content = data_received.c_str();
	const int content_size = data_received.size();

	const int prefix_size = 4;
	if (content_size <= prefix_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}
	uint32_t json_str_size_bg = 0;
	memcpy(&json_str_size_bg, content, prefix_size);

	int json_size = SDL_Swap32(json_str_size_bg);
	if (content_size < prefix_size + json_size) {
		gui2::show_message(null_str, _("Unknown error."));
		return false;
	}

	try {
		std::string json_str(content + prefix_size, json_size);
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(json_str, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		// Json::Value& results = json_object["results"];
		// if (results.isObject()) {
			for (std::vector<std::pair<std::string, std::string> >::const_iterator it = files.begin(); it != files.end(); ++ it) {
				const std::string& key = it->first;
				const std::string& file_name = it->second;

				Json::Value& offset = json_object[key + "_offset"];
				Json::Value& size = json_object[key + "_size"];
				int offset_ = offset.asInt();
				int size_ = size.asInt();
				SDL_MakeDirectory(extract_directory(file_name).c_str());
				tfile file(file_name, GENERIC_WRITE, CREATE_ALWAYS);
				posix_fwrite(file.fp, content + prefix_size + json_size + offset_, size_);
			}
		// }

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}

	return true;
}

bool download_model(const std::string& sessionid, const std::string& token, const std::vector<std::pair<std::string, std::string> >& files)
{
	VALIDATE(!sessionid.empty() && !token.empty() && !files.empty(), null_str);

	net::thttp_agent agent(form_url("aismart/api", "downloadmodel.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_download_model, _2, _3, sessionid, token, boost::ref(files));
	agent.did_post = boost::bind(&did_post_download_model, _2, _3, boost::ref(files));

	return net::handle_http_request(agent);
}


bool did_pre_findmodel(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const bool followed)
{
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["followed"] = followed;

	Json::FastWriter writer;
	body = writer.write(json_root);

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_findmodel(const net::RoseDelegate& delegate, int status, const bool followed, std::set<ttoken_tflite>& items)
{
	items.clear();
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}
		}

		const Json::Value& models = json_object["models"];
		if (models.isArray()) {
			for (int at = 0; at < (int)models.size(); at ++) {
				const Json::Value& item = models[at];
				if (followed) {
					VALIDATE(item["followed"].asBool(), null_str);
				}
				items.insert(ttoken_tflite(item["token"].asString(), item["timestamp"].asInt64(), item["name"].asString(), item["followed"].asBool(), item["nickname"].asString(), 
					item["keywords"].asString(), item["state"].asString(), item["desc"].asString(), item["follows"].asInt(), item["comments"].asInt(), item["size"].asInt()));
			}
		}

	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_findmodel(const std::string& sessionid, const bool followed, std::set<ttoken_tflite>& items)
{
	net::thttp_agent agent(form_url("aismart/api", "findmodel.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_findmodel, _2, _3, sessionid, followed);
	agent.did_post = boost::bind(&did_post_findmodel, _2, _3, followed, boost::ref(items));

	return net::handle_http_request(agent);
}

bool did_pre_vip(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const std::string& sessionid, int op, const std::string& text, const char* image, const int image_len)
{
	VALIDATE(!sessionid.empty(), null_str);
	VALIDATE(op >= 0 && op < nb_vip_op_type && image && image_len > 0, null_str);

	*body = nullptr;
	
	Json::Value json_root;
	json_root["version"] = game_config::version;
	json_root["sessionid"] = sessionid;
	json_root["op"] = vip_op_int_2_json(op);
	json_root["text"] = text;

	int appendix_size = image_len;
	const char* appendix_data = image;

	json_root["image_offset"] = 0;
	json_root["image_size"] = appendix_size;

	Json::FastWriter writer;
	const std::string json_str = writer.write(json_root);
	const int prefix_size = 4;

	const size_t size = prefix_size + json_str.size() + appendix_size;
	char* buf2 = (char*)malloc(size);

	const uint32_t json_str_size_bg = SDL_Swap32(json_str.size());
	memcpy(buf2, &json_str_size_bg, prefix_size);
	memcpy(buf2 + prefix_size, json_str.c_str(), json_str.size());
	memcpy(buf2 + prefix_size + json_str.size(), appendix_data, appendix_size);

	req.set_upload(net::CreateSimpleUploadData(buf2, size));
	*body = buf2;

	headers.SetHeader(net::HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
	return true;
}

bool did_post_vip(const net::RoseDelegate& delegate, int status)
{
	std::stringstream err;

	if (status != net::OK) {
		err << net::err_2_description(status);
		gui2::show_message(null_str, err.str());
		return false;
	}
	const std::string& data_received = delegate.data_received();

	try {
		Json::Reader reader;
		Json::Value json_object;
		if (!reader.parse(data_received, json_object)) {
			err << _("Invalid data.");
		} else {
			Json::Value& response_code = json_object["response_code"];
			int code = response_code.asInt();

			bool handled;
			err << generic_handle_response(code, json_object, handled);
			if (!handled && err.str().empty()) {
				if (code) {
					err << json_object["msg"].asString();
					if (err.str().empty()) {
						err << _("Unknown error.");
					}
				}
			}

			if (err.str().empty()) {
				Json::Value& results = json_object["results"];
				if (results.isObject()) {
					Json::Value& nick = results["nickname"];
					Json::Value& sessionid = results["sessionid"];
					Json::Value& vip = results["vip"];
					Json::Value& vip_ts = results["vip_ts"];

					// current_user.nick = nick.asString();
					// current_user.sessionid = sessionid.asString();
					current_user.vip = vip.asInt();
					current_user.vip_ts = vip_ts.asInt64();
				}
			}
		}
	} catch (const Json::RuntimeError& e) {
		err << e.what();
	} catch (const Json::LogicError& e) {
		err << e.what();
	}

	if (!err.str().empty()) {
		gui2::show_message(null_str, err.str());
		return false;
	}
	return true;
}

bool do_vip(const std::string& sessionid, int op, const std::string& text, const char* image, const int image_len)
{
	char* body = nullptr;
	net::thttp_agent agent(form_url("aismart/api", "vip.do"), "POST", "server.der");
	agent.did_pre = boost::bind(&did_pre_vip, _1, _2, &body, sessionid, op, text, image, image_len);
	agent.did_post = boost::bind(&did_post_vip, _2, _3);

	bool ret = net::handle_http_request(agent);
	if (body) {
		free(body);
	}
	return ret;
}
}