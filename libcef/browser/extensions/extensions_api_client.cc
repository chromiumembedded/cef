// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_api_client.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"
#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"
#include "libcef/browser/printing/print_view_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

namespace extensions {

CefExtensionsAPIClient::CefExtensionsAPIClient() {
}

AppViewGuestDelegate* CefExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  // TODO(extensions): Implement to support Apps.
  NOTREACHED();
  return NULL;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
CefExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  // The GuestViewManager instance associated with the returned Delegate, which
  // will be retrieved in the future via GuestViewManager::FromBrowserContext,
  // will be associated with the CefBrowserContextImpl instead of |context| due
  // to ShouldProxyUserData in browser_context_proxy.cc. Because the
  // GuestViewManagerDelegate keeps a reference to the passed-in context we need
  // to provide the *Impl object instead of |context| which may be a *Proxy
  // object. If we don't do this then the Delegate may attempt to access a
  // *Proxy object that has already been deleted.
  return base::WrapUnique(
      new extensions::ExtensionsGuestViewManagerDelegate(
          CefBrowserContextImpl::GetForContext(context)));
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
CefExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return base::WrapUnique(new CefMimeHandlerViewGuestDelegate(guest));
}

void CefExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  PrefsTabHelper::CreateForWebContents(web_contents);
  printing::CefPrintViewManager::CreateForWebContents(web_contents);

  CefExtensionWebContentsObserver::CreateForWebContents(web_contents);

  // Used by the PDF extension.
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      std::unique_ptr<pdf::PDFWebContentsHelperClient>(
          new CefPDFWebContentsHelperClient()));

  // Used by the tabs extension API.
  SessionTabHelper::CreateForWebContents(web_contents);
  zoom::ZoomController::CreateForWebContents(web_contents);
}

}  // namespace extensions
