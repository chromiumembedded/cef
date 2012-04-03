// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CEF_LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/utility/content_utility_client.h"

class CefContentUtilityClient : public content::ContentUtilityClient {
 public:
  virtual ~CefContentUtilityClient();
  virtual void UtilityThreadStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
};

#endif  // CEF_LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
