// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/extensions/chrome_mime_handler_view_guest_delegate_cef.h"

#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/chrome_context_menu_handler.h"
#include "cef/libcef/browser/osr/web_contents_view_osr.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

ChromeMimeHandlerViewGuestDelegateCef::ChromeMimeHandlerViewGuestDelegateCef(
    MimeHandlerViewGuest* guest)
    : owner_web_contents_(guest->owner_web_contents()) {}

ChromeMimeHandlerViewGuestDelegateCef::
    ~ChromeMimeHandlerViewGuestDelegateCef() = default;

void ChromeMimeHandlerViewGuestDelegateCef::OverrideWebContentsCreateParams(
    content::WebContents::CreateParams* params) {
  DCHECK(params->guest_delegate);

  auto owner_browser =
      CefBrowserHostBase::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  if (owner_browser->IsWindowless()) {
    CefWebContentsViewOSR* view_osr = new CefWebContentsViewOSR(
        owner_browser->GetBackgroundColor(), false, false);
    params->view = view_osr;
    params->delegate_view = view_osr;
  }
}

bool ChromeMimeHandlerViewGuestDelegateCef::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (context_menu::HandleContextMenu(owner_web_contents_, params)) {
    return true;
  }

  [[maybe_unused]] bool result =
      ChromeMimeHandlerViewGuestDelegate::HandleContextMenu(render_frame_host,
                                                            params);
  DCHECK(result);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  ContextMenuDelegate* menu_delegate =
      ContextMenuDelegate::FromWebContents(web_contents);

  // The menu may not be running in the following cases:
  // - If the menu is empty (e.g. cleared in OnBeforeContextMenu).
  // - If the menu is disabled (see e.g. RenderViewContextMenuViews::Show).
  // - When the above call blocks until the menu is dismissed (macOS behavior).
  // We explicitly clean up in these cases instead of waiting for OnMenuClosed
  // which will otherwise never be called for the first 2 cases.
  if (!menu_delegate->IsMenuRunning()) {
    context_menu::MaybeResetContextMenu(owner_web_contents_);
  }

  return true;
}

}  // namespace extensions
