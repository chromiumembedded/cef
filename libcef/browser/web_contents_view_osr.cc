// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/web_contents_view_osr.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/common/drag_data_impl.h"

#include "content/public/browser/render_widget_host.h"

CefWebContentsViewOSR::CefWebContentsViewOSR()
    : web_contents_(NULL),
      view_(NULL) {
}

CefWebContentsViewOSR::~CefWebContentsViewOSR() {
}

void CefWebContentsViewOSR::set_web_contents(
    content::WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
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
  *out = GetViewBounds();
}

void CefWebContentsViewOSR::SizeContents(const gfx::Size& size) {
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
  return view_ ? view_->GetViewBounds() : gfx::Rect();
}

void CefWebContentsViewOSR::CreateView(const gfx::Size& initial_size,
                                       gfx::NativeView context) {
}

content::RenderWidgetHostViewBase* CefWebContentsViewOSR::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host,
    bool is_guest_view_hack) {
  if (render_widget_host->GetView()) {
    return static_cast<content::RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  view_ = new CefRenderWidgetHostViewOSR(render_widget_host);
  return view_;
}

content::RenderWidgetHostViewBase*
    CefWebContentsViewOSR::CreateViewForPopupWidget(
    content::RenderWidgetHost* render_widget_host) {
  return new CefRenderWidgetHostViewOSR(render_widget_host);
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
