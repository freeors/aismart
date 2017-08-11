#ifndef NET_HPP_INCLUDED
#define NET_HPP_INCLUDED

#include "game_config.hpp"

#include <net/url_request/url_request_http_job_rose.hpp>


extern const int max_vericode_size;
extern const int vericode_reserve_secs;
extern const std::string showcase_identifier;

enum {vericode_register, vericode_password, vericode_eraseuser};

struct tuser {
	tuser(const std::string& sessionid = "", const std::string& nick = "", int vip = 0, int64_t vip_ts = 0, const int vip_op = nposm)
		: nick(nick)
		, sessionid(sessionid)
		, vip(vip)
		, vip_ts(vip_ts)
		, vip_op(vip_op)
	{}

	bool valid() const;

	std::string mobile;
	std::string sessionid;
	std::string nick;
	int vip;
	int64_t vip_ts;
	int vip_op;
};

extern const tuser null_user2;
extern tuser current_user;

enum {vip_op_join, vip_op_renew, vip_op_update};
int calculate_vip_op();

bool check_vericode_size(const int size);
bool network_require_login();

namespace net {
std::string form_url(const std::string& category, const std::string& task);

bool did_pre_login(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password);
bool did_post_login(const net::RoseDelegate& delegate, int status, const std::string& mobile, bool quiet, tuser& user);
bool do_login(const std::string& mobile, const std::string& password, bool quiet, tuser& user);

bool did_pre_logout(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid);
bool did_post_logout(const net::RoseDelegate& delegate, int status);
bool do_logout(const std::string& sessionid);

bool did_pre_register(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password, const std::string& vericode);
bool did_post_register(const net::RoseDelegate& delegate, int status, const std::string& mobile, tuser& user);
bool do_register(const std::string& mobile, const std::string& password, const std::string& vericode, tuser& user);

bool did_pre_erase_user(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& vericode);
bool did_post_erase_user(const net::RoseDelegate& delegate, int status);
bool do_erase_user(const std::string& sessionid, const std::string& vericode);

bool did_pre_resetpwd(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, const std::string& password, const std::string& vericode);
bool did_post_resetpwd(const net::RoseDelegate& delegate, int status);
bool do_resetpwd(const std::string& mobile, const std::string& password, const std::string& vericode);

bool did_pre_vericode(net::HttpRequestHeaders& headers, std::string& body, const std::string& mobile, int type);
bool did_post_vericode(const net::RoseDelegate& delegate, int status);
bool do_vericode(const std::string& mobile, int type);

bool did_pre_updateattrs(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::map<std::string, std::string>& attrs);
bool did_post_updateattrs(const net::RoseDelegate& delegate, int status);
bool do_updateattrs(const std::string& sessionid, const std::map<std::string, std::string>& attrs);

bool did_pre_filelist(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid);
bool did_post_filelist(const net::RoseDelegate& delegate, int status, std::set<tnetwork_item>& items);
bool do_filelist(const std::string& sessionid, std::set<tnetwork_item>& items);

bool did_pre_updatemodelattrs(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& name, const std::map<std::string, std::string>& attrs);
bool did_post_updatemodelattrs(const net::RoseDelegate& delegate, int status);
bool do_updatemodelattrs(const std::string& sessionid, const std::string& name, const std::map<std::string, std::string>& attrs);

bool did_pre_upload_file(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files, const int available);
bool did_post_upload_file(const net::RoseDelegate& delegate, int status, int type, std::string& token);
bool upload_file(const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files, const int available, std::string& token);

bool did_pre_download_file(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files);
bool did_post_download_file(const net::RoseDelegate& delegate, int status, const std::vector<std::pair<std::string, std::string> >& files);
bool download_file(const std::string& sessionid, int type, const std::string& name, const std::vector<std::pair<std::string, std::string> >& files);

bool did_pre_deletefile(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, int type, const std::string& name);
bool did_post_deletefile(const net::RoseDelegate& delegate, int status);
bool do_deletefile(const std::string& sessionid, int type, const std::string& name);

enum {op_follow, op_unfollow, op_star};
bool did_pre_followmodel(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, std::string& token, int op);
bool did_post_followmodel(const net::RoseDelegate& delegate, int status);
bool do_followmodel(const std::string& sessionid, std::string& token, int op);

bool did_pre_download_model(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const std::string& token, const std::vector<std::pair<std::string, std::string> >& files);
bool did_post_download_model(const net::RoseDelegate& delegate, int status, const std::vector<std::pair<std::string, std::string> >& files);
bool download_model(const std::string& sessionid, const std::string& token, const std::vector<std::pair<std::string, std::string> >& files);

bool did_pre_findmodel(net::HttpRequestHeaders& headers, std::string& body, const std::string& sessionid, const bool followed);
bool did_post_findmodel(const net::RoseDelegate& delegate, int status, const bool followed, std::set<ttoken_tflite>& items);
bool do_findmodel(const std::string& sessionid, const bool followed, std::set<ttoken_tflite>& items);

bool did_pre_vip(net::URLRequest& req, net::HttpRequestHeaders& headers, char** body, const std::string& sessionid, int op, const std::string& text, const char* image, const int image_len);
bool did_post_vip(const net::RoseDelegate& delegate, int status);
bool do_vip(const std::string& sessionid, int op, const std::string& text, const char* image, const int image_len);
}

#endif