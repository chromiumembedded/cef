// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_TEST_TEST_SERVER_IMPL_H_
#define CEF_LIBCEF_BROWSER_TEST_TEST_SERVER_IMPL_H_
#pragma once

#include <memory>

#include "include/test/cef_test_server.h"

class CefTestServerImpl : public CefTestServer {
 public:
  CefTestServerImpl() = default;

  CefTestServerImpl(const CefTestServerImpl&) = delete;
  CefTestServerImpl& operator=(const CefTestServerImpl&) = delete;

  bool Start(uint16_t port,
             bool https_server,
             cef_test_cert_type_t https_cert_type,
             CefRefPtr<CefTestServerHandler> handler);

  // CefTestServer methods:
  void Stop() override;
  CefString GetOrigin() override;

 private:
  // Only accessed on the creation thread.
  class Context;
  std::unique_ptr<Context> context_;

  // Safe to access on any thread.
  CefString origin_;

  IMPLEMENT_REFCOUNTING(CefTestServerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_TEST_TEST_SERVER_IMPL_H_
