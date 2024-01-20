// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/alloy/alloy_content_browser_client.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/osr/web_contents_view_osr.h"

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

CefMimeHandlerViewGuestDelegate::CefMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest)
    : guest_(guest), owner_web_contents_(guest_->owner_web_contents()) {}

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

void CefMimeHandlerViewGuestDelegate::OnGuestAttached() {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  CefRefPtr<AlloyBrowserHostImpl> owner_browser =
      AlloyBrowserHostImpl::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  // Associate guest state information with the owner browser.
  owner_browser->browser_info()->MaybeCreateFrame(
      web_contents->GetPrimaryMainFrame(), true /* is_guest_view */);
}

void CefMimeHandlerViewGuestDelegate::OnGuestDetached() {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  CefRefPtr<AlloyBrowserHostImpl> owner_browser =
      AlloyBrowserHostImpl::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  // Disassociate guest state information with the owner browser.
  owner_browser->browser_info()->RemoveFrame(
      web_contents->GetPrimaryMainFrame());
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
