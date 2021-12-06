// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_API_PROVIDER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_API_PROVIDER_H_

#include "extensions/browser/extensions_browser_api_provider.h"

namespace extensions {

class CefExtensionsBrowserAPIProvider : public ExtensionsBrowserAPIProvider {
 public:
  CefExtensionsBrowserAPIProvider();

  CefExtensionsBrowserAPIProvider(const CefExtensionsBrowserAPIProvider&) =
      delete;
  CefExtensionsBrowserAPIProvider& operator=(
      const CefExtensionsBrowserAPIProvider&) = delete;

  ~CefExtensionsBrowserAPIProvider() override;

  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry) override;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_API_PROVIDER_H_
