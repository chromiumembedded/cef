// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/url_util.h"

#include "base/logging.h"
#include "components/url_formatter/url_fixer.h"

namespace url_util {

GURL MakeGURL(const CefString& url, bool fixup) {
  GURL gurl = GURL(url.ToString());
  if (!url.empty() && !gurl.is_valid() && !gurl.has_scheme()) {
    std::string fixed_scheme(url::kHttpScheme);
    fixed_scheme.append(url::kStandardSchemeSeparator);
    std::string new_url = url;
    new_url.insert(0, fixed_scheme);
    gurl = GURL(new_url);
  }
  if (fixup) {
    FixupGURL(gurl);
  }
  return gurl;
}

bool FixupGURL(GURL& gurl) {
  if (!gurl.is_empty()) {
    GURL fixup_url =
        url_formatter::FixupURL(gurl.possibly_invalid_spec(), std::string());
    if (fixup_url.is_valid()) {
      gurl = fixup_url;
    } else {
      LOG(ERROR) << "Invalid URL: " << gurl.possibly_invalid_spec();
      gurl = GURL();
      return false;
    }
  }
  return true;
}

}  // namespace url_util
