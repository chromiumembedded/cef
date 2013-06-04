// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/scheme_registration.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/renderer/content_renderer_client.h"

#include "content/public/common/url_constants.h"

namespace scheme {

void AddInternalStandardSchemes(std::vector<std::string>* standard_schemes) {
  static struct {
    const char* name;
    bool is_local;
    bool is_display_isolated;
  } schemes[] = {
    { chrome::kChromeUIScheme,        true,  true  },
    { chrome::kChromeDevToolsScheme,  true,  false }
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    standard_schemes->push_back(schemes[i].name);

    if (CefContentBrowserClient::Get())
      CefContentBrowserClient::Get()->AddCustomScheme(schemes[i].name);

    if (CefContentRendererClient::Get()) {
      CefContentRendererClient::Get()->AddCustomScheme(
          schemes[i].name, true, schemes[i].is_local,
          schemes[i].is_display_isolated);
    }
  }
}

bool IsInternalHandledScheme(const std::string& scheme) {
  static const char* schemes[] = {
    chrome::kBlobScheme,
    chrome::kChromeDevToolsScheme,
    chrome::kChromeUIScheme,
    chrome::kDataScheme,
    chrome::kFileScheme,
    chrome::kFileSystemScheme,
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

bool IsInternalProtectedScheme(const std::string& scheme) {
  // Some of these values originate from StoragePartitionImplMap::Get() in
  // content/browser/storage_partition_impl_map.cc and are modified by
  // InstallInternalProtectedHandlers().
  static const char* schemes[] = {
    chrome::kBlobScheme,
    chrome::kChromeUIScheme,
    chrome::kDataScheme,
    chrome::kFileScheme,
    chrome::kFileSystemScheme,
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

}  // namespace scheme
