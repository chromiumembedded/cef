// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/extensions/chrome_mime_handler_view_guest_delegate_cef.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/chrome/chrome_context_menu_handler.h"

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

ChromeMimeHandlerViewGuestDelegateCef::ChromeMimeHandlerViewGuestDelegateCef(
    MimeHandlerViewGuest* guest)
    : owner_web_contents_(guest->owner_web_contents()) {}

ChromeMimeHandlerViewGuestDelegateCef::
    ~ChromeMimeHandlerViewGuestDelegateCef() = default;

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
