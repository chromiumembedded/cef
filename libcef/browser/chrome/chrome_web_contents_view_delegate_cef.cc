// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_web_contents_view_delegate_cef.h"

#include "cef/libcef/browser/chrome/chrome_context_menu_handler.h"

ChromeWebContentsViewDelegateCef::ChromeWebContentsViewDelegateCef(
    content::WebContents* web_contents)
    : ChromeWebContentsViewDelegateBase(web_contents),
      web_contents_(web_contents) {}

void ChromeWebContentsViewDelegateCef::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (context_menu::HandleContextMenu(web_contents_, params)) {
    return;
  }

  ChromeWebContentsViewDelegateBase::ShowContextMenu(render_frame_host, params);

  // The menu may not be running in the following cases:
  // - If the menu is empty (e.g. cleared in OnBeforeContextMenu).
  // - If the menu is disabled (see e.g. RenderViewContextMenuViews::Show).
  // - When the above call blocks until the menu is dismissed (macOS behavior).
  // We explicitly clean up in these cases instead of waiting for OnMenuClosed
  // which will otherwise never be called for the first 2 cases.
  if (!IsMenuRunning()) {
    context_menu::MaybeResetContextMenu(web_contents_);
  }
}
