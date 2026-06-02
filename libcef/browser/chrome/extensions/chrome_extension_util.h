// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
#define CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
#pragma once

#include <string>

#include "chrome/common/extensions/api/tabs.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

class ExtensionFunction;
class Profile;

namespace extensions {
class URLPatternSet;
}  // namespace extensions

namespace cef {

// Same as ExtensionTabUtil::GetTabById but searching only Alloy style
// CefBrowserHosts.
bool GetAlloyTabById(int tab_id,
                     Profile* profile,
                     bool include_incognito,
                     content::WebContents** contents);

// Returns true if |contents| is owned by an Alloy style CefBrowserHost.
// If |primary_only| is false then guest contents will also be matched.
bool IsAlloyContents(content::WebContents* contents, bool primary_only);

// Records that |popup_contents| (an extension action popup) was opened from an
// Alloy style tab with session tab id |source_tab_id|. The marker is attached
// to |popup_contents| as WebContentsUserData and lives for its lifetime.
void SetAlloyPopupSourceTabId(content::WebContents* popup_contents,
                              int source_tab_id);

// If |function| is a tabs.query coming from an extension action popup hosted on
// behalf of an Alloy style tab (i.e. its sender WebContents carries the marker
// set by SetAlloyPopupSourceTabId), and the query filters allow it, append the
// Alloy tab to |result|. No-op for any other call.
void MaybeAppendAlloyPopupQueryTab(
    ExtensionFunction* function,
    const extensions::api::tabs::Query::Params::QueryInfo& query_info,
    const extensions::URLPatternSet& url_patterns,
    const std::string& window_type,
    int window_id,
    int tab_index,
    base::ListValue* result);

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_EXTENSION_UTIL_H_
