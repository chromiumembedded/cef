// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/scheme_registration.h"
#include "libcef/common/content_client.h"

#include "content/public/common/url_constants.h"

namespace scheme {

void AddInternalSchemes(std::vector<std::string>* standard_schemes) {
  static CefContentClient::SchemeInfo schemes[] = {
    { chrome::kChromeUIScheme,        true,  true,  true  },
    { chrome::kChromeDevToolsScheme,  true,  true,  false }
  };

  CefContentClient* client = CefContentClient::Get();
  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (schemes[0].is_standard)
      standard_schemes->push_back(schemes[i].scheme_name);
    client->AddCustomScheme(schemes[i]);
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
#if !defined(DISABLE_FTP_SUPPORT)
    chrome::kFtpScheme,
#endif
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

}  // namespace scheme
