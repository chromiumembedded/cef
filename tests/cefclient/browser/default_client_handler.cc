// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/default_client_handler.h"

namespace client {

// static
CefRefPtr<DefaultClientHandler> DefaultClientHandler::GetForClient(
    CefRefPtr<CefClient> client) {
  auto base = BaseClientHandler::GetForClient(client);
  if (base && base->GetTypeKey() == &kTypeKey) {
    return static_cast<DefaultClientHandler*>(base.get());
  }
  return nullptr;
}

}  // namespace client
