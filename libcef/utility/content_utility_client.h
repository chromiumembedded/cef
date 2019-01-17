// Copyright (c) 2014 the Chromium Embedded Framework authors.
// Portions Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <memory>
#include <vector>

#include "content/public/utility/content_utility_client.h"

#if defined(OS_WIN)
namespace printing {
class PrintingHandler;
}
#endif

class CefContentUtilityClient : public content::ContentUtilityClient {
 public:
  CefContentUtilityClient();
  ~CefContentUtilityClient() override;

  void UtilityThreadStarted() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool HandleServiceRequest(
      const std::string& service_name,
      service_manager::mojom::ServiceRequest request) override;

 private:
  std::unique_ptr<service_manager::Service> MaybeCreateMainThreadService(
      const std::string& service_name,
      service_manager::mojom::ServiceRequest request);

#if defined(OS_WIN)
  std::unique_ptr<printing::PrintingHandler> printing_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CefContentUtilityClient);
};

#endif  // LIBCEF_UTILITY_CONTENT_UTILITY_CLIENT_H_
