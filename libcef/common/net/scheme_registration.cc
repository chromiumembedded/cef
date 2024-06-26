// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/common/net/scheme_registration.h"

#include "base/containers/contains.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace scheme {

bool IsInternalHandledScheme(const std::string& scheme) {
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

bool IsStandardScheme(const std::string& scheme) {
  url::Component scheme_comp(0, scheme.length());
  return url::IsStandard(scheme.c_str(), scheme_comp);
}

// Should return the same value as SecurityOrigin::isLocal and
// SchemeRegistry::shouldTreatURLSchemeAsCorsEnabled.
bool IsCorsEnabledScheme(const std::string& scheme) {
  return base::Contains(url::GetCorsEnabledSchemes(), scheme);
}

}  // namespace scheme
