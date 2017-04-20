// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/osr/web_contents_view_osr.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

namespace extensions {

namespace {

CefRefPtr<CefBrowserHostImpl> GetOwnerBrowser(
    extensions::MimeHandlerViewGuest* guest) {
  content::WebContents* owner_web_contents = guest->owner_web_contents();
  CefRefPtr<CefBrowserHostImpl> owner_browser =
      CefBrowserHostImpl::GetBrowserForContents(owner_web_contents);
  DCHECK(owner_browser);
  return owner_browser;
}

}  // namespace

CefMimeHandlerViewGuestDelegate::CefMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest)
    : guest_(guest) {
}

CefMimeHandlerViewGuestDelegate::~CefMimeHandlerViewGuestDelegate() {
}

void CefMimeHandlerViewGuestDelegate::OverrideWebContentsCreateParams(
    content::WebContents::CreateParams* params) {
  DCHECK(params->guest_delegate);

  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);
  if (owner_browser->IsWindowless()) {
    CefWebContentsViewOSR* view_osr =
        new CefWebContentsViewOSR(owner_browser->GetBackgroundColor());
    params->view = view_osr;
    params->delegate_view = view_osr;
  }
}

void CefMimeHandlerViewGuestDelegate::OnGuestAttached(
    content::WebContentsView* parent_view) {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  content::RenderViewHost* view_host = web_contents->GetRenderViewHost();
  content::RenderFrameHost* main_frame_host = web_contents->GetMainFrame();

  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);

  // Associate guest state information with the owner browser.
  scoped_refptr<CefBrowserInfo> info = owner_browser->browser_info();
  info->guest_render_id_manager()->add_render_view_id(
      view_host->GetProcess()->GetID(),
      view_host->GetRoutingID());
  info->guest_render_id_manager()->add_render_frame_id(
      main_frame_host->GetProcess()->GetID(),
      main_frame_host->GetRoutingID());
}

void CefMimeHandlerViewGuestDelegate::OnGuestDetached(
    content::WebContentsView* parent_view) {
  content::WebContents* web_contents = guest_->web_contents();
  DCHECK(web_contents);

  content::RenderViewHost* view_host = web_contents->GetRenderViewHost();
  content::RenderFrameHost* main_frame_host = web_contents->GetMainFrame();

  CefRefPtr<CefBrowserHostImpl> owner_browser = GetOwnerBrowser(guest_);

  const int render_process_id = main_frame_host->GetProcess()->GetID();
  const int render_frame_id = main_frame_host->GetRoutingID();
  const bool is_main_frame = (main_frame_host->GetParent() == nullptr);

  // Disassociate guest state information with the owner browser.
  scoped_refptr<CefBrowserInfo> info = owner_browser->browser_info();
  info->guest_render_id_manager()->remove_render_view_id(
      view_host->GetProcess()->GetID(),
      view_host->GetRoutingID());
  info->guest_render_id_manager()->remove_render_frame_id(
      render_process_id, render_frame_id);

  CefBrowserContext* context =
      static_cast<CefBrowserContext*>(web_contents->GetBrowserContext());
  if (context) {
    context->OnRenderFrameDeleted(render_process_id, render_frame_id,
                                  is_main_frame, true);
  }
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
