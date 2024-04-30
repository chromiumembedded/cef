// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/extensions/chrome_mime_handler_view_guest_delegate_cef.h"

#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/chrome_context_menu_handler.h"
#include "cef/libcef/browser/osr/web_contents_view_osr.h"
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

  return ChromeMimeHandlerViewGuestDelegate::HandleContextMenu(
      render_frame_host, params);
}

}  // namespace extensions
