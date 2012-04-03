// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/utility/content_utility_client.h"

CefContentUtilityClient::~CefContentUtilityClient() {
}

void CefContentUtilityClient::UtilityThreadStarted() {
}

bool CefContentUtilityClient::OnMessageReceived(const IPC::Message& message) {
  return false;
}
