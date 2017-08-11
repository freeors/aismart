// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_SSL_CONFIG_SERVICE_BINDCERT_H_
#define NET_SSL_SSL_CONFIG_SERVICE_BINDCERT_H_

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

	class NET_EXPORT SSLConfigServiceBindCert : public SSLConfigService {
	public:
		SSLConfigServiceBindCert(const std::string& cert);

		// Store default SSL config settings in |config|.
		void GetSSLConfig(SSLConfig* config) override;

	private:
		~SSLConfigServiceBindCert() override;

		// Default value of prefs.
		const SSLConfig default_config_;

		static std::map<std::string, std::string> certs;
		const std::string cert_;

		DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceBindCert);
	};

}  // namespace net

#endif  // NET_SSL_SSL_CONFIG_SERVICE_BINDCERT_H_