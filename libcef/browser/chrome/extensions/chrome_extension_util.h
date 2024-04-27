// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
#define CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
#pragma once

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace cef {

// Same as ExtensionTabUtil::GetTabById but searching only Alloy style
// CefBrowserHosts.
bool GetAlloyTabById(int tab_id,
                     Profile* profile,
                     bool include_incognito,
                     content::WebContents** contents);

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
