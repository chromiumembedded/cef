// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_browser_api_provider.h"
#include "libcef/browser/extensions/chrome_api_registration.h"

// #include "cef/libcef/browser/extensions/api/generated_api_registration.h"
#include "extensions/browser/api/generated_api_registration.h"

namespace extensions {

CefExtensionsBrowserAPIProvider::CefExtensionsBrowserAPIProvider() = default;
CefExtensionsBrowserAPIProvider::~CefExtensionsBrowserAPIProvider() = default;

void CefExtensionsBrowserAPIProvider::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  // CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  // api::cef::CefGeneratedFunctionRegistry::RegisterAll(registry);

  // Chrome APIs whitelisted by CEF.
  api::cef::ChromeFunctionRegistry::RegisterAll(registry);
}

}  // namespace extensions
