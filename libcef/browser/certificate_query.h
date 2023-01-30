// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CERTIFICATE_QUERY_H_
#define CEF_LIBCEF_BROWSER_CERTIFICATE_QUERY_H_
#pragma once

#include "base/functional/callback_forward.h"
#include "content/public/browser/certificate_request_result_type.h"

namespace content {
class WebContents;
}

namespace net {
class SSLInfo;
}

class GURL;

namespace certificate_query {

using CertificateErrorCallback =
    base::OnceCallback<void(content::CertificateRequestResultType)>;

// Called from ContentBrowserClient::AllowCertificateError.
// |callback| will be returned if the request is unhandled and
// |default_disallow| is false.
[[nodiscard]] CertificateErrorCallback AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    CertificateErrorCallback callback,
    bool default_disallow);

}  // namespace certificate_query

#endif  // CEF_LIBCEF_BROWSER_CERTIFICATE_QUERY_H_
