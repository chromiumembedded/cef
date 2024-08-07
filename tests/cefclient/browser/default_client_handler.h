// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#pragma once

#include "tests/cefclient/browser/base_client_handler.h"

namespace client {

// Default client handler for unmanaged browser windows. Used with the Chrome
// runtime only.
class DefaultClientHandler : public BaseClientHandler {
 public:
  DefaultClientHandler() = default;

  // Returns the DefaultClientHandler for |client|, or nullptr if |client| is
  // not a DefaultClientHandler.
  static CefRefPtr<DefaultClientHandler> GetForClient(
      CefRefPtr<CefClient> client);

 private:
  // Used to determine the object type.
  virtual const void* GetTypeKey() const override { return &kTypeKey; }
  static constexpr int kTypeKey = 0;

  IMPLEMENT_REFCOUNTING(DefaultClientHandler);
  DISALLOW_COPY_AND_ASSIGN(DefaultClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
