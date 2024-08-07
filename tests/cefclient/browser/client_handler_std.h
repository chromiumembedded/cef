// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_STD_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_STD_H_
#pragma once

#include "tests/cefclient/browser/client_handler.h"

namespace client {

// Client handler implementation for windowed browsers. There will only ever be
// one browser per handler instance.
class ClientHandlerStd : public ClientHandler {
 public:
  ClientHandlerStd(Delegate* delegate,
                   bool with_controls,
                   const std::string& startup_url);

  // Returns the ClientHandlerStd for |client|, or nullptr if |client| is not a
  // ClientHandlerStd.
  static CefRefPtr<ClientHandlerStd> GetForClient(CefRefPtr<CefClient> client);

 private:
  // Used to determine the object type.
  virtual const void* GetTypeKey() const override { return &kTypeKey; }
  static constexpr int kTypeKey = 0;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientHandlerStd);
  DISALLOW_COPY_AND_ASSIGN(ClientHandlerStd);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_HANDLER_STD_H_
