// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_

namespace content {
class WebContents;
}

namespace extensions {

// Returns the WebContents that owns the specified |guest|, if any.
content::WebContents* GetGuestForOwnerContents(content::WebContents* guest);

// Returns the guest WebContents for the specified |owner|, if any.
content::WebContents* GetOwnerForGuestContents(content::WebContents* owner);

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
