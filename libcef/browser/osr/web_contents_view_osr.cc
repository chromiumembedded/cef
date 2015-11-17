// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/web_contents_view_osr.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/common/drag_data_impl.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/user_metrics.h"

CefWebContentsViewOSR::CefWebContentsViewOSR(bool transparent)
    : transparent_(transparent),
      web_contents_(NULL),
      view_(NULL),
      guest_(NULL) {
}

CefWebContentsViewOSR::~CefWebContentsViewOSR() {
}

void CefWebContentsViewOSR::set_web_contents(
    content::WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
}

void CefWebContentsViewOSR::set_guest(content::BrowserPluginGuest* guest) {
  DCHECK(!guest_);
  guest_ = guest;
}

gfx::NativeView CefWebContentsViewOSR::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeView CefWebContentsViewOSR::GetContentNativeView() const {
  return gfx::NativeView();
}

gfx::NativeWindow CefWebContentsViewOSR::GetTopLevelNativeWindow() const {
  return gfx::NativeWindow();
}

void CefWebContentsViewOSR::GetContainerBounds(gfx::Rect* out) const {
  if (guest_) {
    // Based on WebContentsViewGuest::GetContainerBounds.
    if (guest_->embedder_web_contents()) {
      // We need embedder container's bounds to calculate our bounds.
      guest_->embedder_web_contents()->GetView()->GetContainerBounds(out);
      gfx::Point guest_coordinates = guest_->GetScreenCoordinates(gfx::Point());
      out->Offset(guest_coordinates.x(), guest_coordinates.y());
    } else {
      out->set_origin(gfx::Point());
    }
    out->set_size(size_);
    return;
  }

  *out = GetViewBounds();
}

void CefWebContentsViewOSR::SizeContents(const gfx::Size& size) {
  if (guest_) {
    // Based on WebContentsViewGuest::SizeContents.
    size_ = size;
    content::RenderWidgetHostView* rwhv =
        web_contents_->GetRenderWidgetHostView();
    if (rwhv)
      rwhv->SetSize(size);
  }
}

void CefWebContentsViewOSR::Focus() {
}

void CefWebContentsViewOSR::SetInitialFocus() {
}

void CefWebContentsViewOSR::StoreFocus() {
}

void CefWebContentsViewOSR::RestoreFocus() {
}

content::DropData* CefWebContentsViewOSR::GetDropData() const {
  return NULL;
}

gfx::Rect CefWebContentsViewOSR::GetViewBounds() const {
  if (guest_) {
    // Based on WebContentsViewGuest::GetViewBounds.
    return gfx::Rect(size_);
  }

  return view_ ? view_->GetViewBounds() : gfx::Rect();
}

void CefWebContentsViewOSR::CreateView(const gfx::Size& initial_size,
                                       gfx::NativeView context) {
  if (guest_) {
    // Based on WebContentsViewGuest::CreateView.
    size_ = initial_size;
  }
}

content::RenderWidgetHostViewBase* CefWebContentsViewOSR::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  if (render_widget_host->GetView()) {
    return static_cast<content::RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  if (guest_) {
    // Based on WebContentsViewGuest::CreateViewForWidget.
    content::WebContents* embedder_web_contents =
        guest_->embedder_web_contents();
    CefRenderWidgetHostViewOSR* embedder_host_view =
        static_cast<CefRenderWidgetHostViewOSR*>(
            embedder_web_contents->GetRenderViewHost()->GetWidget()->GetView());

    CefRenderWidgetHostViewOSR* platform_widget =
        new CefRenderWidgetHostViewOSR(transparent_, render_widget_host,
                                       embedder_host_view);
    embedder_host_view->AddGuestHostView(platform_widget);

    return new content::RenderWidgetHostViewGuest(
        render_widget_host,
        guest_,
        platform_widget->GetWeakPtr());
  }

  view_ = new CefRenderWidgetHostViewOSR(transparent_, render_widget_host,
                                         NULL);
  return view_;
}

// Called for popup and fullscreen widgets.
content::RenderWidgetHostViewBase*
    CefWebContentsViewOSR::CreateViewForPopupWidget(
    content::RenderWidgetHost* render_widget_host) {
  return new CefRenderWidgetHostViewOSR(transparent_, render_widget_host,
                                        view_);
}

void CefWebContentsViewOSR::SetPageTitle(const base::string16& title) {
}

void CefWebContentsViewOSR::RenderViewCreated(content::RenderViewHost* host) {
  if (view_)
    view_->InstallTransparency();
}

void CefWebContentsViewOSR::RenderViewSwappedIn(
    content::RenderViewHost* host) {
}

void CefWebContentsViewOSR::SetOverscrollControllerEnabled(bool enabled) {
}

#if defined(OS_MACOSX)
void CefWebContentsViewOSR::SetAllowOtherViews(bool allow) {
}

bool CefWebContentsViewOSR::GetAllowOtherViews() const {
  return false;
}

bool CefWebContentsViewOSR::IsEventTracking() const {
  return false;
}

void CefWebContentsViewOSR::CloseTabAfterEventTracking() {
}
#endif  // defined(OS_MACOSX)

void CefWebContentsViewOSR::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info) {
  if (guest_) {
    // Based on WebContentsViewGuest::StartDragging.
    content::WebContentsImpl* embedder_web_contents =
        guest_->embedder_web_contents();
    embedder_web_contents->GetBrowserPluginEmbedder()->StartDrag(guest_);
    content::RenderViewHostImpl* embedder_render_view_host =
        static_cast<content::RenderViewHostImpl*>(
            embedder_web_contents->GetRenderViewHost());
    CHECK(embedder_render_view_host);
    content::RenderViewHostDelegateView* view =
        embedder_render_view_host->GetDelegate()->GetDelegateView();
    if (view) {
      content::RecordAction(
          base::UserMetricsAction("BrowserPlugin.Guest.StartDrag"));
      view->StartDragging(drop_data, allowed_ops, image, image_offset,
                          event_info);
    } else {
      embedder_web_contents->SystemDragEnded();
    }
    return;
  }

  CefRefPtr<CefBrowserHostImpl> browser;
  CefRefPtr<CefRenderHandler> handler;
  bool handled = false;
  CefRenderWidgetHostViewOSR* view =
      static_cast<CefRenderWidgetHostViewOSR*>(view_);
  if (view)
    browser = view->browser_impl();
  if (browser.get())
    handler = browser->GetClient()->GetRenderHandler();
  if (handler.get()) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(drop_data));
    drag_data->SetReadOnly(true);
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    handled = handler->StartDragging(
        browser.get(),
        drag_data.get(),
        static_cast<CefRenderHandler::DragOperationsMask>(allowed_ops),
        event_info.event_location.x(),
        event_info.event_location.y());
  }
  if (!handled && web_contents_)
    web_contents_->SystemDragEnded();
}

void CefWebContentsViewOSR::UpdateDragCursor(
    blink::WebDragOperation operation) {
  if (guest_) {
    // Based on WebContentsViewGuest::UpdateDragCursor.
    content::RenderViewHostImpl* embedder_render_view_host =
        static_cast<content::RenderViewHostImpl*>(
            guest_->embedder_web_contents()->GetRenderViewHost());
    CHECK(embedder_render_view_host);
    content::RenderViewHostDelegateView* view =
        embedder_render_view_host->GetDelegate()->GetDelegateView();
    if (view)
      view->UpdateDragCursor(operation);
    return;
  }

  CefRefPtr<CefBrowserHostImpl> browser;
  CefRefPtr<CefRenderHandler> handler;
  CefRenderWidgetHostViewOSR* view =
      static_cast<CefRenderWidgetHostViewOSR*>(view_);
  if (view)
    browser = view->browser_impl();
  if (browser.get())
    handler = browser->GetClient()->GetRenderHandler();
  if (handler.get()) {
    handler->UpdateDragCursor(
        browser.get(),
        static_cast<CefRenderHandler::DragOperation>(operation));
  }
}
