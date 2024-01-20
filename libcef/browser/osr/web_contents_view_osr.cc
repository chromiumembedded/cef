// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/web_contents_view_osr.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/osr/touch_selection_controller_client_osr.h"
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
      use_external_begin_frame_(use_external_begin_frame) {}

CefWebContentsViewOSR::~CefWebContentsViewOSR() = default;

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
    if (view) {
      view->InstallTransparency();
    }
  }
}

gfx::Rect CefWebContentsViewOSR::GetContainerBounds() const {
  return GetViewBounds();
}

void CefWebContentsViewOSR::GotFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl) {
      web_contents_impl->NotifyWebContentsFocused(render_widget_host);
    }
  }
}

void CefWebContentsViewOSR::LostFocus(
    content::RenderWidgetHostImpl* render_widget_host) {
  if (web_contents_) {
    content::WebContentsImpl* web_contents_impl =
        static_cast<content::WebContentsImpl*>(web_contents_);
    if (web_contents_impl) {
      web_contents_impl->NotifyWebContentsLostFocus(render_widget_host);
    }
  }
}

void CefWebContentsViewOSR::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate()) {
    web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse);
  }
}

gfx::Rect CefWebContentsViewOSR::GetViewBounds() const {
  CefRenderWidgetHostViewOSR* view = GetView();
  return view ? view->GetViewBounds() : gfx::Rect();
}

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

void CefWebContentsViewOSR::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  auto selection_controller_client = GetSelectionControllerClient();
  if (selection_controller_client &&
      selection_controller_client->HandleContextMenu(params)) {
    // Context menu display, if any, will be handled via
    // AlloyWebContentsViewDelegate::ShowContextMenu.
    return;
  }

  if (auto browser = GetBrowser()) {
    browser->ShowContextMenu(params);
  }
}

void CefWebContentsViewOSR::StartDragging(
    const content::DropData& drop_data,
    const url::Origin& source_origin,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset,
    const gfx::Rect& drag_obj_rect,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->StartDragging(drop_data, allowed_ops, image, cursor_offset,
                           event_info, source_rwh);
  } else if (web_contents_) {
    static_cast<content::WebContentsImpl*>(web_contents_)
        ->SystemDragEnded(source_rwh);
  }
}

void CefWebContentsViewOSR::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  CefRefPtr<AlloyBrowserHostImpl> browser = GetBrowser();
  if (browser.get()) {
    browser->UpdateDragOperation(operation, document_is_handling_drag);
  }
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
  if (view) {
    return view->browser_impl().get();
  }
  return nullptr;
}

CefTouchSelectionControllerClientOSR*
CefWebContentsViewOSR::GetSelectionControllerClient() const {
  CefRenderWidgetHostViewOSR* view = GetView();
  return view ? view->selection_controller_client() : nullptr;
}
