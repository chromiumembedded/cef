// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#pragma once

#include "tests/cefclient/browser/base_client_handler.h"

namespace client {

// Default client handler for unmanaged browser windows. Used with Chrome
// style only.
class DefaultClientHandler : public BaseClientHandler {
 public:
  DefaultClientHandler() = default;

 private:
  IMPLEMENT_REFCOUNTING(DefaultClientHandler);
  DISALLOW_COPY_AND_ASSIGN(DefaultClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
