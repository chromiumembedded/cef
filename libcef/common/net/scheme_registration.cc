// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/scheme_registration.h"

#include "libcef/common/content_client.h"

#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "url/url_constants.h"

namespace scheme {

void AddInternalSchemes(std::vector<std::string>* standard_schemes,
                        std::vector<std::string>* savable_schemes) {
  // chrome: and chrome-devtools: schemes are registered in
  // RenderThreadImpl::RegisterSchemes().
  // Access restrictions for chrome-extension: and chrome-extension-resource:
  // schemes will be applied in CefContentRendererClient::WillSendRequest().
  static CefContentClient::SchemeInfo schemes[] = {
    { extensions::kExtensionScheme,         true,  true,  false, false },
    { extensions::kExtensionResourceScheme, true,  true,  false, false },
  };

  CefContentClient* client = CefContentClient::Get();
  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (schemes[i].is_standard)
      standard_schemes->push_back(schemes[i].scheme_name);
    if (schemes[i].is_savable)
      savable_schemes->push_back(schemes[i].scheme_name);
    client->AddCustomScheme(schemes[i]);
  }
}

bool IsInternalHandledScheme(const std::string& scheme) {
  static const char* schemes[] = {
    url::kBlobScheme,
    content::kChromeDevToolsScheme,
    content::kChromeUIScheme,
    extensions::kExtensionScheme,
    extensions::kExtensionResourceScheme,
    url::kDataScheme,
    url::kFileScheme,
    url::kFileSystemScheme,
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
    url::kBlobScheme,
    content::kChromeUIScheme,
    extensions::kExtensionScheme,
    extensions::kExtensionResourceScheme,
    url::kDataScheme,
    url::kFileScheme,
    url::kFileSystemScheme,
#if !defined(DISABLE_FTP_SUPPORT)
    url::kFtpScheme,
#endif
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

}  // namespace scheme
