// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_api_client.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"
#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"
#include "libcef/browser/printing/print_view_manager.h"

#include "components/pdf/browser/pdf_web_contents_helper.h"

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

void CefExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  printing::PrintViewManager::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new CefPDFWebContentsHelperClient()));
  CefExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

}  // namespace extensions
