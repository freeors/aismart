// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define GETTEXT_DOMAIN "rose-lib"

#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_http_job_rose.hpp"

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "net/base/auth.h"
#include "net/base/request_priority.h"
#include "net/cert/ct_policy_status.h"
#include "net/http/http_transaction_factory.h"
#include "net/net_features.h"
#include "net/socket/client_socket_factory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_rose_util.h"

#include "net/base/upload_bytes_element_reader.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/net_errors.h"

#include "gui/dialogs/dialog.hpp"
#include "serialization/string_utils.hpp"
#include "filesystem.hpp"
#include "wml_exception.hpp"
#include "rose_config.hpp"
#include "gettext.hpp"

namespace net {

class URLRequestHttpJobWithProxy 
{
public:
	explicit URLRequestHttpJobWithProxy(std::unique_ptr<ProxyResolutionService> proxy_resolution_service, const std::string& cert)
		: proxy_resolution_service_(std::move(proxy_resolution_service))
		, context_(new RoseURLRequestContext(cert)) 
	{
		context_->set_client_socket_factory(ClientSocketFactory::GetDefaultFactory());
		context_->set_network_delegate(&network_delegate_);
		context_->set_proxy_resolution_service(proxy_resolution_service_.get());
		context_->Init();
	}

	TestNetworkDelegate network_delegate_;
	std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
	std::unique_ptr<RoseURLRequestContext> context_;

private:
	DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJobWithProxy);
};

struct turlrequest_instance_lock
{
	turlrequest_instance_lock(URLRequest& req)
	{
		VALIDATE(!URLRequest::http_instance, null_str);
		URLRequest::http_instance = &req;
	}

	~turlrequest_instance_lock()
	{
		VALIDATE(URLRequest::http_instance, null_str);
		URLRequest::http_instance = nullptr;
	}
};

std::unique_ptr<UploadDataStream> CreateSimpleUploadData(const char* data, int size) 
{
  std::unique_ptr<UploadElementReader> reader(new UploadBytesElementReader(data, size));
  return ElementsUploadDataStream::CreateWithReader(std::move(reader), 0);
}

std::string err_2_description(int err)
{
	VALIDATE(err != OK, null_str);

	if (err == ERR_IO_PENDING) {
		return _("Pending");

	} else if (err == ERR_TIMED_OUT) {
		return _("Timed out");

	} else if (err == ERR_ABORTED) {
		return _("Can not connect to network. Please check if the Internet is connected.");

	} else if (err == ERR_NAME_NOT_RESOLVED) {
		return _("Can not connect to network. Please check if the Internet is connected.");
	}
	return _("Other error");
}

thttp_agent::thttp_agent(const std::string& _url, const std::string& _method, const std::string& _cert)
	: url(_url)
	, method(_method)
	, cert(_cert)
{
	const std::string scheme = url.scheme();
	VALIDATE(scheme == "http" || scheme == "https", null_str);

	const std::string host = url.host();
	VALIDATE(host.size() >= 2, null_str);
}

void show_slice(const tshow_slice_executor* executor)
{
	if (executor->cancel) {
		return;
	}
	executor->progress->show_slice();
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, executor));
}

bool handle_http_request(thttp_agent& agent)
{
	URLRequestHttpJobWithProxy http_job_with_proxy(nullptr, agent.cert);
	// URLRequestHttpJobWithProxy http_job_with_proxy(nullptr, null_str);

	RoseDelegate delegate;
	std::unique_ptr<URLRequest> req = http_job_with_proxy.context_->CreateRequest(
		  agent.url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
	turlrequest_instance_lock lock(*req.get());

	if (agent.method != "GET") {
		req->set_method(agent.method);
	}

	// used in whole http session. elements_upload_data_stream don't release heap.
	std::string req_body;
	if (agent.did_pre) {
		HttpRequestHeaders headers;
		if (!agent.did_pre(boost::ref(*req.get()), boost::ref(headers), boost::ref(req_body))) {
			return false;
		}
		if (!headers.IsEmpty()) {
			req->SetExtraRequestHeaders(headers);
		}
		if (!req_body.empty()) {
			req->set_upload(net::CreateSimpleUploadData(req_body.c_str(), req_body.size()));
		}
	}

	req->Start();
	CHECK(req->is_pending());
	
	base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, base::Bind(&show_slice, &delegate.executor()));

	base::RunLoop().Run();

	if (agent.did_post) {
		return agent.did_post(boost::ref(*req.get()), boost::ref(delegate), delegate.request_status());
	}
	return delegate.request_status() == OK;

	// CHECK(ProxyServer::Direct() == request->proxy_server());
	// request->was_fetched_via_proxy()
}

// @url: https://www.github.com
// @path: 1.html --> <Documents>/RoseApp/<appid>/1.html
void fetch_url_data(const std::string& _url, const std::string& path)
{
	VALIDATE(!path.empty() && path[0] != '/', null_str);

	const GURL url(_url);
	const std::string scheme = url.scheme();
	VALIDATE(scheme == "http" || scheme == "https", null_str);

	URLRequestHttpJobWithProxy http_job_with_proxy(nullptr, null_str);

	RoseDelegate delegate;
	std::unique_ptr<URLRequest> req = http_job_with_proxy.context_->CreateRequest(
			url, DEFAULT_PRIORITY, &delegate, TRAFFIC_ANNOTATION_FOR_TESTS);
	turlrequest_instance_lock lock(*req.get());

	req->Start();
	CHECK(req->is_pending());
	base::RunLoop().Run();

	std::string data = delegate.data_received();
	if (delegate.request_status() != OK) {
		std::stringstream err;
		err << "Fetch from " << _url << " fail. error: " << delegate.request_status();
		data = err.str();
	}
	
	tfile file(game_config::preferences_dir + "/" + path, GENERIC_WRITE, CREATE_ALWAYS);
	posix_fwrite(file.fp, data.c_str(), data.size());
}

}  // namespace net