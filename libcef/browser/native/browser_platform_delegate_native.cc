// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native.h"

#include "libcef/browser/browser_host_impl.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"

CefBrowserPlatformDelegateNative::CefBrowserPlatformDelegateNative(
    const CefWindowInfo& window_info)
    : window_info_(window_info),
      windowless_handler_(nullptr) {
}

void CefBrowserPlatformDelegateNative::WasResized() {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->WasResized();
}

void CefBrowserPlatformDelegateNative::SendKeyEvent(
    const content::NativeWebKeyboardEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardKeyboardEvent(event);
}

void CefBrowserPlatformDelegateNative::SendMouseEvent(
    const blink::WebMouseEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardMouseEvent(event);
}

void CefBrowserPlatformDelegateNative::SendMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->GetWidget()->ForwardWheelEvent(event);
}

bool CefBrowserPlatformDelegateNative::IsWindowless() const {
  return false;
}
