// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_

#include <vector>

#include "include/internal/cef_ptr.h"

#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class AlloyBrowserHostImpl;

namespace extensions {

class Extension;

// Returns the WebContents that owns the specified |guest|, if any.
content::WebContents* GetOwnerForGuestContents(
    const content::WebContents* guest);

// Test for different types of guest contents.
bool IsBrowserPluginGuest(const content::WebContents* web_contents);
bool IsPrintPreviewDialog(const content::WebContents* web_contents);

// Returns the browser matching |tab_id| and |browser_context|. Returns false if
// |tab_id| is < 0 or a matching browser cannot be found within
// |browser_context|. Similar in concept to ExtensionTabUtil::GetTabById.
CefRefPtr<AlloyBrowserHostImpl> GetBrowserForTabId(
    int tab_id,
    content::BrowserContext* browser_context);

// Returns the extension associated with |url| in |profile|. Returns nullptr
// if the extension does not exist.
const Extension* GetExtensionForUrl(content::BrowserContext* browser_context,
                                    const GURL& url);

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_BROWSER_EXTENSIONS_UTIL_H_
