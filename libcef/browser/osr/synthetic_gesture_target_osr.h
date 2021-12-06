// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_SYNTHETIC_GESTURE_TARGET_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_SYNTHETIC_GESTURE_TARGET_OSR_H_

#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"

// SyntheticGestureTarget implementation for OSR.
class CefSyntheticGestureTargetOSR
    : public content::SyntheticGestureTargetBase {
 public:
  explicit CefSyntheticGestureTargetOSR(content::RenderWidgetHostImpl* host);

  CefSyntheticGestureTargetOSR(const CefSyntheticGestureTargetOSR&) = delete;
  CefSyntheticGestureTargetOSR& operator=(const CefSyntheticGestureTargetOSR&) =
      delete;

  // SyntheticGestureTargetBase:
  void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebGestureEventToPlatform(
      const blink::WebGestureEvent& web_gesture,
      const ui::LatencyInfo& latency_info) override;
  void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) override;

  // SyntheticGestureTarget:
  content::mojom::GestureSourceType GetDefaultSyntheticGestureSourceType()
      const override;
  float GetTouchSlopInDips() const override;
  float GetSpanSlopInDips() const override;
  float GetMinScalingSpanInDips() const override;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_SYNTHETIC_GESTURE_TARGET_OSR_H_
