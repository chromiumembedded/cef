// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/common/client_app.h"

namespace client {

// Stub implementations of ClientApp methods that are only used in the browser
// process.

// static
void ClientApp::CreateBrowserDelegates(BrowserDelegateSet& delegates) {
}

// static
CefRefPtr<CefPrintHandler> ClientApp::CreatePrintHandler() {
  return NULL;
}

}  // namespace client
