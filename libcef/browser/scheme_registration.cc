// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/scheme_registration.h"
#include "libcef/browser/chrome_scheme_handler.h"
#include "libcef/browser/devtools_scheme_handler.h"
#include "libcef/renderer/content_renderer_client.h"

namespace scheme {

void AddStandardSchemes(std::vector<std::string>* standard_schemes) {
  static struct {
    const char* name;
    bool is_local;
    bool is_display_isolated;
  } schemes[] = {
    { scheme::kChromeScheme,          true,  true  },
    { scheme::kChromeDevToolsScheme,  true,  false }
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i)
    standard_schemes->push_back(schemes[i].name);

  if (CefContentRendererClient::Get()) {
    // Running in single-process mode. Register the schemes with WebKit.
    for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
      CefContentRendererClient::Get()->AddCustomScheme(
          schemes[i].name, schemes[i].is_local, schemes[i].is_display_isolated);
    }
  }
}

void RegisterInternalHandlers() {
  scheme::RegisterChromeHandler();
  scheme::RegisterChromeDevToolsHandler();
}

void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url) {
  if (validated_url.scheme() == scheme::kChromeScheme)
    scheme::DidFinishChromeLoad(frame, validated_url);
}

}  // namespace scheme
