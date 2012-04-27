// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_app.h"
#include "cefclient/dom_test.h"
#include "cefclient/scheme_test.h"

// static
void ClientApp::CreateRenderDelegates(RenderDelegateSet& delegates) {
  dom_test::CreateRenderDelegates(delegates);
}

// static
void ClientApp::RegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar) {
  scheme_test::RegisterCustomSchemes(registrar);
}
