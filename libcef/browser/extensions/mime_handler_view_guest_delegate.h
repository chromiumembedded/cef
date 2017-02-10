// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"

namespace content {
struct ContextMenuParams;
}

namespace extensions {

class CefMimeHandlerViewGuestDelegate : public MimeHandlerViewGuestDelegate {
 public:
  explicit CefMimeHandlerViewGuestDelegate(MimeHandlerViewGuest* guest);
  ~CefMimeHandlerViewGuestDelegate() override;

  // MimeHandlerViewGuestDelegate methods.
  void OverrideWebContentsCreateParams(
      content::WebContents::CreateParams* params) override;
  void OnGuestAttached(content::WebContentsView* parent_view) override;
  void OnGuestDetached(content::WebContentsView* parent_view) override;
  bool HandleContextMenu(content::WebContents* web_contents,
                         const content::ContextMenuParams& params) override;

 private:
  MimeHandlerViewGuest* guest_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(CefMimeHandlerViewGuestDelegate);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
