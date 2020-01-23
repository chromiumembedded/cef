// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

CefPDFWebContentsHelperClient::CefPDFWebContentsHelperClient() {}

CefPDFWebContentsHelperClient::~CefPDFWebContentsHelperClient() {}

void CefPDFWebContentsHelperClient::UpdateContentRestrictions(
    content::WebContents* contents,
    int content_restrictions) {}

void CefPDFWebContentsHelperClient::OnPDFHasUnsupportedFeature(
    content::WebContents* contents) {}

void CefPDFWebContentsHelperClient::OnSaveURL(content::WebContents* contents) {}

void CefPDFWebContentsHelperClient::SetPluginCanSave(
    content::WebContents* contents,
    bool can_save) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(contents);
  if (guest_view)
    guest_view->SetPluginCanSave(can_save);
}

}  // namespace extensions
