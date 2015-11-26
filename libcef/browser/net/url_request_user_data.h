// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_USER_DATA_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_USER_DATA_H_

#include "include/cef_base.h"
#include "base/supports_user_data.h"

#include "include/cef_urlrequest.h"

// Used to annotate all URLRequests for which the request can be associated
// with the CefURLRequestClient.
class CefURLRequestUserData : public base::SupportsUserData::Data {
 public:
  CefURLRequestUserData(CefRefPtr<CefURLRequestClient> client);
  ~CefURLRequestUserData() override;

  CefRefPtr<CefURLRequestClient> GetClient();
  static const void* kUserDataKey;

private:
  CefRefPtr<CefURLRequestClient> client_;
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_USER_DATA_H_
