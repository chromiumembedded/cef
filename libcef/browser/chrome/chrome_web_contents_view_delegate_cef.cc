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
}
