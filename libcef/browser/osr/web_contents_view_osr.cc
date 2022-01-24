// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/web_contents_view_osr.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/common/drag_data_impl.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"

CefWebContentsViewOSR::CefWebContentsViewOSR(SkColor background_color,
                                             bool use_shared_texture,
                                             bool use_external_begin_frame)
    : background_color_(background_color),
      use_shared_texture_(use_shared_texture),
      use_external_begin_frame_(use_external_begin_frame),
      web_contents_(nullptr) {}

CefWebContentsViewOSR::~CefWebContentsViewOSR() {}

void CefWebContentsViewOSR::WebContentsCreated(
    content::WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;

  RenderViewCreated();
}

void CefWebContentsViewOSR::RenderViewCreated() {
  if (web_contents_) {
    auto host = web_contents_->GetRenderViewHost();
    CefRenderWidgetHostViewOSR* view =
        static_cast<CefRenderWidgetHostViewOSR*>(host->GetWidget()->GetView());
    if (view)
      view->InstallTransparency();
  }
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

gfx::Rect CefWebContentsViewOSR::GetContainerBounds() const {
  return GetViewBounds();
}

void CefWebContentsViewOSR::Focus() {}

void CefWebContentsViewOSR::SetInitialFocus() {}

void CefWebContentsViewOSR::StoreFocus() {}

void CefWebContentsViewOSR::RestoreFocus() {}

void CefWebContentsViewOSR::FocusThroughTabTraversal(bool reverse) {}

void CefWebContentsViewOSR::GotFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl)
      web_contents_impl->NotifyWebContentsFocused(render_widget_host);
  }
}

void CefWebContentsViewOSR::LostFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl)
      web_contents_impl->NotifyWebContentsLostFocus(render_widget_host);
  }
}

void CefWebContentsViewOSR::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse);
}

content::DropData* CefWebContentsViewOSR::GetDropData() const {
  return nullptr;
}

gfx::Rect CefWebContentsViewOSR::GetViewBounds() const {
  CefRenderWidgetHostViewOSR* view = GetView();
  return view ? view->GetViewBounds() : gfx::Rect();
}

void CefWebContentsViewOSR::CreateView(gfx::NativeView context) {}

content::RenderWidgetHostViewBase* CefWebContentsViewOSR::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    return static_cast<content::RenderWidgetHostViewBase*>(
        render_widget_host->GetView());
  }

  return new CefRenderWidgetHostViewOSR(background_color_, use_shared_texture_,
                                        use_external_begin_frame_,
                                        render_widget_host, nullptr);
}

// Called for popup and fullscreen widgets.
content::RenderWidgetHostViewBase*
CefWebContentsViewOSR::CreateViewForChildWidget(
    content::RenderWidgetHost* render_widget_host) {
  CefRenderWidgetHostViewOSR* view = GetView();
  CHECK(view);

  return new CefRenderWidgetHostViewOSR(background_color_, use_shared_texture_,
                                        use_external_begin_frame_,
                                        render_widget_host, view);
}

void CefWebContentsViewOSR::SetPageTitle(const std::u16string& title) {}

void CefWebContentsViewOSR::RenderViewReady() {
  RenderViewCreated();
}

void CefWebContentsViewOSR::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {}

void CefWebContentsViewOSR::SetOverscrollControllerEnabled(bool enabled) {}

void CefWebContentsViewOSR::OnCapturerCountChanged() {}

#if BUILDFLAG(IS_MAC)
bool CefWebContentsViewOSR::CloseTabAfterEventTrackingIfNeeded() {
  return false;
}
#endif  // BUILDFLAG(IS_MAC)

void CefWebContentsViewOSR::StartDragging(
    const content::DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->StartDragging(drop_data, allowed_ops, image, image_offset,
                           event_info, source_rwh);
  } else if (web_contents_) {
    static_cast<content::WebContentsImpl*>(web_contents_)
        ->SystemDragEnded(source_rwh);
  }
}

void CefWebContentsViewOSR::UpdateDragCursor(
    ui::mojom::DragOperation operation) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
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

AlloyBrowserHostImpl* CefWebContentsViewOSR::GetBrowser() const {
  CefRenderWidgetHostViewOSR* view = GetView();
  if (view)
    return view->browser_impl().get();
  return nullptr;
}
