// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native.h"

#include "libcef/browser/browser_host_impl.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

CefBrowserPlatformDelegateNative::CefBrowserPlatformDelegateNative(
    const CefWindowInfo& window_info,
    SkColor background_color,
    bool use_shared_texture,
    bool use_external_begin_frame)
    : window_info_(window_info),
      background_color_(background_color),
      use_shared_texture_(use_shared_texture),
      use_external_begin_frame_(use_external_begin_frame),
      windowless_handler_(nullptr) {}

SkColor CefBrowserPlatformDelegateNative::GetBackgroundColor() const {
  return background_color_;
}

bool CefBrowserPlatformDelegateNative::CanUseSharedTexture() const {
  return use_shared_texture_;
}

bool CefBrowserPlatformDelegateNative::CanUseExternalBeginFrame() const {
  return use_external_begin_frame_;
}

void CefBrowserPlatformDelegateNative::WasResized() {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->SynchronizeVisualProperties();
}

bool CefBrowserPlatformDelegateNative::IsWindowless() const {
  return false;
}

bool CefBrowserPlatformDelegateNative::IsViewsHosted() const {
  return false;
}
