// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/extensions/mime_handler_view_guest_delegate.h"

#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/osr/web_contents_view_osr.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

CefMimeHandlerViewGuestDelegate::CefMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest)
    : owner_web_contents_(guest->owner_web_contents()) {}

CefMimeHandlerViewGuestDelegate::~CefMimeHandlerViewGuestDelegate() = default;

void CefMimeHandlerViewGuestDelegate::OverrideWebContentsCreateParams(
    content::WebContents::CreateParams* params) {
  DCHECK(params->guest_delegate);

  CefRefPtr<AlloyBrowserHostImpl> owner_browser =
      AlloyBrowserHostImpl::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  if (owner_browser->IsWindowless()) {
    CefWebContentsViewOSR* view_osr = new CefWebContentsViewOSR(
        owner_browser->GetBackgroundColor(), false, false);
    params->view = view_osr;
    params->delegate_view = view_osr;
  }
}

bool CefMimeHandlerViewGuestDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  CefRefPtr<AlloyBrowserHostImpl> owner_browser =
      AlloyBrowserHostImpl::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  return owner_browser->ShowContextMenu(params);
}

}  // namespace extensions
