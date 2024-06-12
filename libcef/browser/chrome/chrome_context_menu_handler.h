// Copyright (c) 2021 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTEXT_MENU_HANDLER_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTEXT_MENU_HANDLER_H_
#pragma once

#include "content/public/browser/context_menu_params.h"

namespace content {
class WebContents;
}

namespace context_menu {

// Register RenderViewContextMenu callbacks.
void RegisterCallbacks();

// Returns true if the menu was handled.
bool HandleContextMenu(content::WebContents* opener,
                       const content::ContextMenuParams& params);

void MaybeResetContextMenu(content::WebContents* opener);

}  // namespace context_menu

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTEXT_MENU_HANDLER_H_
