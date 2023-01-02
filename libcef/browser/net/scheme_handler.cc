// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net/scheme_handler.h"

#include <string>

#include "libcef/browser/net/chrome_scheme_handler.h"
#include "libcef/browser/net/devtools_scheme_handler.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/features/runtime.h"

#include "content/public/common/url_constants.h"

namespace scheme {

void RegisterInternalHandlers(CefIOThreadState* iothread_state) {
  if (!cef::IsAlloyRuntimeEnabled()) {
    return;
  }

  scheme::RegisterChromeDevToolsHandler(iothread_state);
}

}  // namespace scheme
