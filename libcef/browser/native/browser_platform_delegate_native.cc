// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/native/browser_platform_delegate_native.h"

#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

CefBrowserPlatformDelegateNative::CefBrowserPlatformDelegateNative(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : window_info_(window_info), background_color_(background_color) {}

SkColor CefBrowserPlatformDelegateNative::GetBackgroundColor() const {
  return background_color_;
}

void CefBrowserPlatformDelegateNative::WasResized() {
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (host) {
    host->GetWidget()->SynchronizeVisualProperties();
  }
}

void CefBrowserPlatformDelegateNative::NotifyScreenInfoChanged() {
  content::RenderWidgetHostImpl* render_widget_host = nullptr;
  if (web_contents_) {
    if (auto* rvh = web_contents_->GetRenderViewHost()) {
      render_widget_host =
          content::RenderWidgetHostImpl::From(rvh->GetWidget());
    }
  }
  if (!render_widget_host) {
    return;
  }

  // Send updated screen bounds information to the renderer process.
  if (render_widget_host->delegate()) {
    render_widget_host->delegate()->SendScreenRects();
  } else {
    render_widget_host->SendScreenRects();
  }

  render_widget_host->NotifyScreenInfoChanged();
}
