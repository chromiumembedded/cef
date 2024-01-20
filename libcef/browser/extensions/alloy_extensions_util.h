// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_ALLOY_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_ALLOY_EXTENSIONS_UTIL_H_

namespace content {
class WebContents;
}

namespace extensions::alloy {

// Returns the tabId for |web_contents|, or -1 if not found.
int GetTabIdForWebContents(content::WebContents* web_contents);

}  // namespace extensions::alloy

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_ALLOY_EXTENSIONS_UTIL_H_
