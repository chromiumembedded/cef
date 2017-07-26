// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <memory>
#include <vector>

#include "content/public/utility/content_utility_client.h"

class UtilityMessageHandler;

class CefContentUtilityClient : public content::ContentUtilityClient {
 public:
  CefContentUtilityClient();
  ~CefContentUtilityClient() override;

  void UtilityThreadStarted() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void RegisterServices(StaticServiceMap* services) override;

 private:
  using Handlers = std::vector<std::unique_ptr<UtilityMessageHandler>>;
  Handlers handlers_;

  DISALLOW_COPY_AND_ASSIGN(CefContentUtilityClient);
};

#endif  // LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
