// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/native/browser_platform_delegate_native.h"

#include "base/logging.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/browser_event_util.h"
#include "cef/libcef/browser/osr/motion_event_osr.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/blink/blink_event_util.h"

CefBrowserPlatformDelegateNative::CefBrowserPlatformDelegateNative(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : window_info_(window_info), background_color_(background_color) {}

CefBrowserPlatformDelegateNative::~CefBrowserPlatformDelegateNative() = default;

void CefBrowserPlatformDelegateNative::SendTouchEvent(
    const CefTouchEvent& event) {
  if (!web_contents_) {
    return;
  }
  auto* host = content::RenderWidgetHostImpl::From(
      web_contents_->GetPrimaryMainFrame()->GetRenderWidgetHost());
  auto* view = host->GetView();
  if (!view || view->destroy_pending()) {
    return;
  }
  if (!IsValidTouchEvent(event)) {
    LOG(WARNING) << "SendTouchEvent: Invalid input event";
    return;
  }
  const auto bounds = view->GetViewBounds();
  if (event.type == CEF_TET_PRESSED &&
      (event.x < 0 || event.y < 0 || event.x >= bounds.width() ||
       event.y >= bounds.height())) {
    return;
  }
  if (touch_view_.get() != view) {
    touch_view_ = view->GetWeakPtr();
    touch_state_ = std::make_unique<CefMotionEventOSR>();
  }
  if (!touch_state_->OnTouch(event)) {
    return;
  }
  auto touch =
      ui::CreateWebTouchEventFromMotionEvent(*touch_state_, false, false);
  if (event.type == CEF_TET_CANCELLED) {
    // Blink and native gesture recognizers cancel the entire touch sequence.
    // Keep every point cancelled and discard all tracked contacts together.
    touch_state_ = std::make_unique<CefMotionEventOSR>();
  } else {
    touch_state_->MarkUnchangedTouchPointsAsStationary(&touch, event);
    touch_state_->CleanupRemovedTouchPoints(event);
  }
  const auto origin = bounds.origin();
  for (unsigned i = 0; i < touch.touches_length; ++i) {
    const auto point = touch.touches[i].PositionInWidget();
    touch.touches[i].SetPositionInScreen(point.x() + origin.x(),
                                         point.y() + origin.y());
  }

  SendWebTouchEvent(touch);
}

input::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNative::TranslateWebKeyboardInputEvent(
    const blink::WebKeyboardEvent& event) const {
  return CreateNativeKeyboardInputEvent(event, gfx::NativeView());
}

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
