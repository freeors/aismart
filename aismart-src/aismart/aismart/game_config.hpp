#ifndef GAME_CONFIG_H_INCLUDED
#define GAME_CONFIG_H_INCLUDED

#include "preferences.hpp"
#include "sdl_utils.hpp"
#include "gui/widgets/widget.hpp"
#include <google/protobuf/message.h>

enum {xmit_script, xmit_tflite, xmit_image};
enum {source_camera, source_img};
enum {scenario_classifier, scenario_detect, scenario_ocr, scenario_pr};

struct tscript
{
	tscript()
		: test(false)
		, res(false)
		, id()
		, source(nposm)
		, scenario(nposm)
		, width(0)
		, height(0)
		, color(nposm)
		, kthreshold(0)
	{}

	tscript(const std::string& file, bool res, std::string& err_report);

	bool from(const config& cfg, std::string& err_report);
	bool valid() const { return !id.empty() && !tflite.empty() && scenario != nposm && color != nposm; }

	std::string path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::path + "/" + game_config::app_dir: game_config::preferences_dir);
		ss << "/tflites/" + id;
		return ss.str();
	}

	bool operator<(const tscript& that) const 
	{
		if (!!test != !!that.test) {
			return test;
		}
		VALIDATE(!test && !that.test, null_str);

		if (!!res != !!that.res) {
			return !res;
		}
		int cmp = strcmp(id.c_str(), that.id.c_str());
		return cmp < 0;
	}

	bool res;
	bool test;
	std::string id;

	std::string tflite;
	std::string warn;
	int source;
	std::string img;
	int scenario;
	int width;
	int height;
	int color;
	float kthreshold;
};
extern const tscript null_script;

struct ttflite
{
	ttflite(const std::string& id, bool res)
		: id(id)
		, res(res)
	{}

	bool valid() const { return !id.empty(); }

	std::string path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::path + "/" + game_config::app_dir: game_config::preferences_dir);
		ss << "/tflites/" + id;
		return ss.str();
	}

	std::string tflits_path() const 
	{
		VALIDATE(valid(), null_str);

		std::stringstream ss;
		ss << (res? game_config::path + "/" + game_config::app_dir: game_config::preferences_dir);
		ss << "/tflites/" + id + "/" + id + ".tflits";
		return ss.str();
	}

	bool aes_encrypt(const std::string& key) const;
	bool aes_decrypt(const std::string& key) const;

	bool operator<(const ttflite& that) const 
	{
		if (!!res != !!that.res) {
			return !res;
		}
		int cmp = strcmp(id.c_str(), that.id.c_str());
		return cmp < 0;
	}

	std::string id;
	bool res;
};

struct treadme
{
	static std::map<std::string, std::string> support_keywords;
	static std::map<std::string, std::string> support_states;

	static void prepare_stats_keywords();
	static const std::string& keyword_name(const std::string& id);
	static std::string keywords_name(const std::vector<std::string>& ids);

	static const std::string& state_name(const std::string& id);
};

struct tnetwork_item 
{
	explicit tnetwork_item(const std::string& name, int size, int xmit_type)
		: name(name)
		, size(size)
		, xmit_type(xmit_type)
		, timestamp(0)
		, follows(0)
		, comments(0)
		, showcase(false)
	{}

	void set_model_special(const std::string& _token, const int64_t _timestamp, const std::string& state_id, const std::string& keywords_id, const std::string& _desc, const bool _showcase, const int _follows, const int _comments);

	bool operator<(const tnetwork_item& that) const 
	{
		if (xmit_type != that.xmit_type) {
			if (xmit_type == xmit_script) {
				return true;
			} else if (xmit_type == xmit_tflite) {
				if (that.xmit_type == xmit_script) {
					return false;
				}
				return true;
			}
			return false;
		}
		int cmp = strcmp(name.c_str(), that.name.c_str());
		return cmp < 0;
	}

	std::string name;
	int size;
	int xmit_type;

	// model special
	std::string token;
	int64_t timestamp;
	std::string state;
	std::vector<std::string> keywords;
	std::string description;
	int follows;
	int comments;
	bool showcase;
};

struct ttoken_tflite
{
	explicit ttoken_tflite(const std::string& token, const int timestamp, const std::string& name, const bool followed, const std::string& nickname, const std::string& keywords_id, const std::string& state_id, const std::string& desc, int follows, int comments, int size);

	bool operator<(const ttoken_tflite& that) const 
	{
		if (follows != that.follows) {
			return follows > that.follows;
		}
		if (comments != that.comments) {
			return comments > that.comments;
		}

		const int name_cmp = strcmp(name.c_str(), that.name.c_str());
		if (name_cmp) {
			return name_cmp < 0;
		}

		const int nickname_cmp = strcmp(nickname.c_str(), that.nickname.c_str());
		if (nickname_cmp) {
			return nickname_cmp < 0;
		}

		const int token_cmp = strcmp(token.c_str(), that.token.c_str());
		return token_cmp < 0;
	}

	const std::string token;
	int64_t timestamp;
	std::string name;
	bool followed;
	const std::string nickname;
	int follows;
	int comments;
	int size;

	std::vector<std::string> keywords;
	std::string state;
	std::string description;
};

//basic game configuration information is here.
namespace game_config {

extern const std::string aismart_key;
extern const std::string showcase_app_secret;
extern std::string showcase_sha256;

}

namespace preferences {
	std::string script();
	void set_script(const std::string& value);

	std::string bbs_server_furl();
	void set_bbs_server_furl(const std::string& furl);
}

#endif
