// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/url_request_user_data.h"

CefURLRequestUserData::CefURLRequestUserData(
    CefRefPtr<CefURLRequestClient> client)
    : client_(client) {}

CefURLRequestUserData::~CefURLRequestUserData() {}

CefRefPtr<CefURLRequestClient> CefURLRequestUserData::GetClient() {
  return client_;
}

// static
const void* CefURLRequestUserData::kUserDataKey =
    static_cast<const void*>(&CefURLRequestUserData::kUserDataKey);
