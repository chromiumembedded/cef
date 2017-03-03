// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/scheme_registration.h"

#include "libcef/common/content_client.h"

#include "content/public/common/url_constants.h"
#include "net/net_features.h"
#include "extensions/common/constants.h"
#include "url/url_constants.h"

namespace scheme {

void AddInternalSchemes(content::ContentClient::Schemes* schemes) {
  // chrome: and chrome-devtools: schemes are registered in
  // RenderThreadImpl::RegisterSchemes().
  // Access restrictions for chrome-extension: and chrome-extension-resource:
  // schemes will be applied in CefContentRendererClient::WillSendRequest().
  static CefContentClient::SchemeInfo internal_schemes[] = {
    {
      extensions::kExtensionScheme,
      true,   /* is_standard */
      false,  /* is_local */
      false,  /* is_display_isolated */
      true,   /* is_secure */
      true,   /* is_cors_enabled */
      true,   /* is_csp_bypassing */
    },
  };

  // The |is_display_isolated| value is excluded here because it's registered
  // with Blink only.
  CefContentClient* client = CefContentClient::Get();
  for (size_t i = 0; i < sizeof(internal_schemes) / sizeof(internal_schemes[0]);
       ++i) {
    if (internal_schemes[i].is_standard)
      schemes->standard_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_local)
      schemes->local_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_secure)
      schemes->secure_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_cors_enabled)
      schemes->cors_enabled_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_csp_bypassing)
      schemes->csp_bypassing_schemes.push_back(internal_schemes[i].scheme_name);
    client->AddCustomScheme(internal_schemes[i]);
  }
}

bool IsInternalHandledScheme(const std::string& scheme) {
  static const char* schemes[] = {
    url::kBlobScheme,
    content::kChromeDevToolsScheme,
    content::kChromeUIScheme,
    extensions::kExtensionScheme,
    url::kDataScheme,
    url::kFileScheme,
    url::kFileSystemScheme,
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kFtpScheme,
#endif
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
    url::kDataScheme,
    url::kFileScheme,
    url::kFileSystemScheme,
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
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
