// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_CEF_H_
#define CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_CEF_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/guest_view/mime_handler_view/chrome_mime_handler_view_guest_delegate.h"

namespace extensions {

class MimeHandlerViewGuest;

class ChromeMimeHandlerViewGuestDelegateCef
    : public ChromeMimeHandlerViewGuestDelegate {
 public:
  explicit ChromeMimeHandlerViewGuestDelegateCef(MimeHandlerViewGuest* guest);

  ChromeMimeHandlerViewGuestDelegateCef(
      const ChromeMimeHandlerViewGuestDelegateCef&) = delete;
  ChromeMimeHandlerViewGuestDelegateCef& operator=(
      const ChromeMimeHandlerViewGuestDelegateCef&) = delete;

  ~ChromeMimeHandlerViewGuestDelegateCef() override;

  // MimeHandlerViewGuestDelegate methods.
  void OverrideWebContentsCreateParams(
      content::WebContents::CreateParams* params) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

 private:
  raw_ptr<content::WebContents> owner_web_contents_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_CHROME_EXTENSIONS_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_CEF_H_
