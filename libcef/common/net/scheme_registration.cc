// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/common/net/scheme_registration.h"

#include <algorithm>

#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace scheme {

bool IsInternalHandledScheme(std::string_view scheme) {
  static const char* schemes[] = {
      url::kAboutScheme,
      url::kBlobScheme,
      content::kChromeDevToolsScheme,
      content::kChromeUIScheme,
      content::kChromeUIUntrustedScheme,
      url::kDataScheme,
      extensions::kExtensionScheme,
      url::kFileScheme,
      url::kFileSystemScheme,
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kJavaScriptScheme,
      url::kWsScheme,
      url::kWssScheme,
  };

  for (auto& i : schemes) {
    if (scheme == i) {
      return true;
    }
  }

  return false;
}

bool IsStandardScheme(std::string_view scheme) {
  return url::IsStandard(scheme);
}

// Should return the same value as SecurityOrigin::isLocal and
// SchemeRegistry::shouldTreatURLSchemeAsCorsEnabled.
bool IsCorsEnabledScheme(std::string_view scheme) {
  return std::ranges::contains(url::GetCorsEnabledSchemes(), scheme);
}

}  // namespace scheme
