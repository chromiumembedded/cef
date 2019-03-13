// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net/net_util.h"

#include "net/url_request/url_request.h"
#include "url/url_constants.h"

namespace net_util {

bool IsInternalRequest(const net::URLRequest* request) {
  // With PlzNavigate we now receive blob URLs. Ignore these URLs.
  // See https://crbug.com/776884 for details.
  if (request->url().SchemeIs(url::kBlobScheme)) {
    return true;
  }

  return false;
}

}  // namespace net_util
