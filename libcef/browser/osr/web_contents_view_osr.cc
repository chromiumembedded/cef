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

CefWebContentsViewOSR::CefWebContentsViewOSR(SkColor background_color)
    : background_color_(background_color),
      web_contents_(NULL) {
}

CefWebContentsViewOSR::~CefWebContentsViewOSR() {
}

void CefWebContentsViewOSR::WebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;

  // Call this again for popup browsers now that the view should exist.
  RenderViewCreated(web_contents_->GetRenderViewHost());
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

void CefWebContentsViewOSR::GetScreenInfo(content::ScreenInfo* results) const {
  CefRenderWidgetHostViewOSR* view = GetView();
  if (view)
    view->GetScreenInfo(results);
  else
    WebContentsView::GetDefaultScreenInfo(results);
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
  CefRenderWidgetHostViewOSR* view = GetView();
  return view ? view->GetViewBounds() : gfx::Rect();
}

void CefWebContentsViewOSR::CreateView(const gfx::Size& initial_size,
                                       gfx::NativeView context) {
}

content::RenderWidgetHostViewBase* CefWebContentsViewOSR::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host,
    content::RenderWidgetHost* embedder_render_widget_host) {
  if (render_widget_host->GetView()) {
    return static_cast<content::RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  CefRenderWidgetHostViewOSR* embedder_host_view = nullptr;
  if (embedder_render_widget_host) {
    embedder_host_view = static_cast<CefRenderWidgetHostViewOSR*>(
        embedder_render_widget_host->GetView());
  }

  const bool is_guest_view_hack = !!embedder_render_widget_host;
  return new CefRenderWidgetHostViewOSR(background_color_, render_widget_host,
                                        embedder_host_view, is_guest_view_hack);
}

// Called for popup and fullscreen widgets.
content::RenderWidgetHostViewBase*
    CefWebContentsViewOSR::CreateViewForPopupWidget(
    content::RenderWidgetHost* render_widget_host) {
  CefRenderWidgetHostViewOSR* view = GetView();
  CHECK(view);

  return new CefRenderWidgetHostViewOSR(background_color_, render_widget_host,
                                        view, false);
}

void CefWebContentsViewOSR::SetPageTitle(const base::string16& title) {
}

void CefWebContentsViewOSR::RenderViewCreated(content::RenderViewHost* host) {
  // |view| will be nullptr the first time this method is called for popup
  // browsers.
  CefRenderWidgetHostViewOSR* view = GetView();
  if (view)
    view->InstallTransparency();
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
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  CefRefPtr<CefBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->StartDragging(drop_data, allowed_ops, image, image_offset,
                           event_info, source_rwh);
  } else if (web_contents_) {
    web_contents_->SystemDragEnded(source_rwh);
  }
}

void CefWebContentsViewOSR::UpdateDragCursor(
    blink::WebDragOperation operation) {
  CefRefPtr<CefBrowserHostImpl> browser = GetBrowser();
  if (browser.get())
    browser->UpdateDragCursor(operation);
}

CefRenderWidgetHostViewOSR* CefWebContentsViewOSR::GetView() const {
  if (web_contents_) {
    return static_cast<CefRenderWidgetHostViewOSR*>(
        web_contents_->GetRenderViewHost()->GetWidget()->GetView());
  }
  return nullptr;
}

CefBrowserHostImpl* CefWebContentsViewOSR::GetBrowser() const {
  CefRenderWidgetHostViewOSR* view = GetView();
  if (view)
    return view->browser_impl().get();
  return nullptr;
}
