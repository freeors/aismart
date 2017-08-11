// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

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
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#include "net/base/upload_bytes_element_reader.h"
#include "net/base/elements_upload_data_stream.h"

#if defined(OS_ANDROID)
// #include "base/android/jni_android.h"
// #include "jni/AndroidNetworkLibraryTestUtil_jni.h"
#endif

namespace net {

class URLRequestHttpJobWithProxy {
 public:
  explicit URLRequestHttpJobWithProxy(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service)
      : proxy_resolution_service_(std::move(proxy_resolution_service)),
        context_(new TestURLRequestContext(true)) {
	context_->set_client_socket_factory(ClientSocketFactory::GetDefaultFactory());
    context_->set_network_delegate(&network_delegate_);
    context_->set_proxy_resolution_service(proxy_resolution_service_.get());
    context_->Init();
  }

  TestNetworkDelegate network_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  std::unique_ptr<TestURLRequestContext> context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJobWithProxy);
};

static std::unique_ptr<UploadDataStream> CreateSimpleUploadData(const char* data) {
  std::unique_ptr<UploadElementReader> reader(
      new UploadBytesElementReader(data, strlen(data)));
  return ElementsUploadDataStream::CreateWithReader(std::move(reader), 0);
}

void test_chromium()
{
  URLRequestHttpJobWithProxy http_job_with_proxy(nullptr);

  const char kData[] = "{\"mobile\":\"13111112223\",\"password\":\"123456\",\"version\":\"1.0.1\"}";

  TestDelegate delegate;
  std::unique_ptr<URLRequest> request =
      http_job_with_proxy.context_->CreateRequest(
		  // GURL("http://133.130.113.73/aismart/api/login.do"), DEFAULT_PRIORITY, &delegate,
		  // GURL("http://www.leagor.com/aismart/api/login.do"), DEFAULT_PRIORITY, &delegate,
		  GURL("https://www.github.com"), DEFAULT_PRIORITY, &delegate,
		  // GURL("https://www.apple.com/"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS);
/*
  request->set_method("POST");
  request->set_upload(CreateSimpleUploadData(kData));

  HttpRequestHeaders headers;
  headers.SetHeader(HttpRequestHeaders::kContentType, "application/json; charset=UTF-8");
  request->SetExtraRequestHeaders(headers);
*/
  request->Start();
  CHECK(request->is_pending());
  base::RunLoop().Run();

  int ii = 0;

  request->Start();
  CHECK(request->is_pending());
  base::RunLoop().Run();

  CHECK(ProxyServer::Direct() == request->proxy_server());
  // request->was_fetched_via_proxy()
}

}  // namespace net
