// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/browser/web_contents_view_osr.h"

#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

namespace {

CefRefPtr<CefBrowserHostImpl> GetOwnerBrowser(
    extensions::MimeHandlerViewGuest* guest) {
  content::WebContents* owner_web_contents = guest->GetOwnerWebContents();
  CefRefPtr<CefBrowserHostImpl> owner_browser =
      CefBrowserHostImpl::GetBrowserForContents(owner_web_contents);
  DCHECK(owner_browser);
  return owner_browser;
}

}  // namespace

CefMimeHandlerViewGuestDelegate::CefMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest)
    : MimeHandlerViewGuestDelegate(guest), guest_(guest) {
}

CefMimeHandlerViewGuestDelegate::~CefMimeHandlerViewGuestDelegate() {
}

void CefMimeHandlerViewGuestDelegate::OverrideWebContentsCreateParams(
    content::WebContents::CreateParams* params) {
  DCHECK(params->guest_delegate);

  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);
  if (owner_browser->IsWindowless()) {
    CefWebContentsViewOSR* view_osr = new CefWebContentsViewOSR();
    params->view = view_osr;
    params->delegate_view = view_osr;
  }
}

bool CefMimeHandlerViewGuestDelegate::OnGuestAttached(
    content::WebContentsView* guest_view,
    content::WebContentsView* parent_view) {
  // Do nothing when the browser is windowless.
  return GetOwnerBrowser(guest_)->IsWindowless();
}

bool CefMimeHandlerViewGuestDelegate::OnGuestDetached(
    content::WebContentsView* guest_view,
    content::WebContentsView* parent_view) {
  // Do nothing when the browser is windowless.
  return GetOwnerBrowser(guest_)->IsWindowless();
}

bool CefMimeHandlerViewGuestDelegate::CreateViewForWidget(
    content::WebContentsView* guest_view,
    content::RenderWidgetHost* render_widget_host) {
  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);
  if (owner_browser->IsWindowless()) {
    static_cast<CefWebContentsViewOSR*>(guest_view)->CreateViewForWidget(
        render_widget_host, true);
    return true;
  }
  return false;
}

// TODO(lazyboy): Investigate ways to move this out to /extensions.
void CefMimeHandlerViewGuestDelegate::AttachHelpers() {
  content::WebContents* web_contents = guest_->web_contents();

  // Associate state information with the new WebContents.
  content::RenderViewHost* view_host = web_contents->GetRenderViewHost();
  content::RenderFrameHost* main_frame_host = web_contents->GetMainFrame();
  scoped_refptr<CefBrowserInfo> info =
      CefContentBrowserClient::Get()->GetOrCreateBrowserInfo(
          view_host->GetProcess()->GetID(),
          view_host->GetRoutingID(),
          main_frame_host->GetProcess()->GetID(),
          main_frame_host->GetRoutingID());
  info->set_mime_handler_view(true);

  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);
  if (owner_browser->IsWindowless()) {
    // Use the OSR view instead of the default WebContentsViewGuest.
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents);
    CefWebContentsViewOSR* view_osr =
        static_cast<CefWebContentsViewOSR*>(
            web_contents_impl->GetView());
    view_osr->set_web_contents(web_contents);
    view_osr->set_guest(web_contents_impl->GetBrowserPluginGuest());
  }

  printing::PrintViewManager::CreateForWebContents(web_contents);
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents,
      scoped_ptr<pdf::PDFWebContentsHelperClient>(
          new CefPDFWebContentsHelperClient()));
  CefExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

bool CefMimeHandlerViewGuestDelegate::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  content::ContextMenuParams new_params = params;

  gfx::Point guest_coordinates =
      static_cast<content::WebContentsImpl*>(web_contents)->
          GetBrowserPluginGuest()->GetScreenCoordinates(gfx::Point());

  // Adjust (x,y) position for offset from guest to embedder.
  new_params.x += guest_coordinates.x();
  new_params.y += guest_coordinates.y();

  return GetOwnerBrowser(guest_)->HandleContextMenu(web_contents, new_params);
}

}  // namespace extensions
