// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/client_app.h"

// static
void ClientApp::CreateRenderDelegates(RenderDelegateSet& delegates) {
  // Bring in the process message tests.
  extern void CreateProcessMessageRendererTests(
      ClientApp::RenderDelegateSet& delegates);
  CreateProcessMessageRendererTests(delegates);

  // Bring in the V8 tests.
  extern void CreateV8RendererTests(ClientApp::RenderDelegateSet& delegates);
  CreateV8RendererTests(delegates);
}
