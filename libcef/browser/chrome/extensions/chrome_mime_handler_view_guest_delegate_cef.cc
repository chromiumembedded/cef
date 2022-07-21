// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/extensions/chrome_mime_handler_view_guest_delegate_cef.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info.h"

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

ChromeMimeHandlerViewGuestDelegateCef::ChromeMimeHandlerViewGuestDelegateCef(
    MimeHandlerViewGuest* guest)
    : guest_(guest), owner_web_contents_(guest_->owner_web_contents()) {}

ChromeMimeHandlerViewGuestDelegateCef::
    ~ChromeMimeHandlerViewGuestDelegateCef() = default;

void ChromeMimeHandlerViewGuestDelegateCef::OnGuestAttached() {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  auto owner_browser =
      CefBrowserHostBase::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  // Associate guest state information with the owner browser.
  owner_browser->browser_info()->MaybeCreateFrame(
      web_contents->GetPrimaryMainFrame(), true /* is_guest_view */);
}

void ChromeMimeHandlerViewGuestDelegateCef::OnGuestDetached() {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  auto owner_browser =
      CefBrowserHostBase::GetBrowserForContents(owner_web_contents_);
  DCHECK(owner_browser);

  // Disassociate guest state information with the owner browser.
  owner_browser->browser_info()->RemoveFrame(
      web_contents->GetPrimaryMainFrame());
}

}  // namespace extensions
