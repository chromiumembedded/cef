// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_ACTION_IMPL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_ACTION_IMPL_H_
#pragma once

#include <string>

#include "cef/include/cef_base.h"
#include "cef/include/cef_image.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace cef {

// Returns the popup URL the extension's action declares for |tab_id|, or an
// empty GURL if there is no popup (e.g. the action only dispatches a click).
GURL GetExtensionActionPopupUrl(content::BrowserContext* browser_context,
                                const extensions::Extension& extension,
                                int tab_id);

// Returns the current toolbar action icon for |extension_id| as a CefImage at
// its native 16-DIP size (with any higher-resolution representations the
// extension supplies), or null if the extension or its action is not available.
// Must be called on the UI thread.
CefRefPtr<CefImage> GetExtensionActionIcon(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    int tab_id);

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_ACTION_IMPL_H_
