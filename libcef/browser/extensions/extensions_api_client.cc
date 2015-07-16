// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_api_client.h"

#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"

namespace extensions {

CefExtensionsAPIClient::CefExtensionsAPIClient() {
}

AppViewGuestDelegate* CefExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  // TODO(extensions): Implement to support Apps.
  NOTREACHED();
  return NULL;
}

scoped_ptr<MimeHandlerViewGuestDelegate>
CefExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return make_scoped_ptr(new CefMimeHandlerViewGuestDelegate(guest));
}

}  // namespace extensions
