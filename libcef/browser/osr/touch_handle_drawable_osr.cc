// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/touch_handle_drawable_osr.h"

#include <cmath>

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace {
// Copied from touch_handle_drawable_aura.cc

// The distance by which a handle image is offset from the focal point (i.e.
// text baseline) downwards.
constexpr int kSelectionHandleVerticalVisualOffset = 2;

// The padding around the selection handle image can be used to extend the
// handle so that touch events near the selection handle image are
// targeted to the selection handle.
constexpr int kSelectionHandlePadding = 0;

}  // namespace

int CefTouchHandleDrawableOSR::counter_ = 0;

CefTouchHandleDrawableOSR::CefTouchHandleDrawableOSR(
    CefRenderWidgetHostViewOSR* rwhv)
    : rwhv_(rwhv), id_(counter_++) {}

void CefTouchHandleDrawableOSR::SetEnabled(bool enabled) {
  if (enabled == enabled_) {
    return;
  }

  enabled_ = enabled;

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ENABLED;
  touch_handle_state.enabled = enabled_;
  TouchHandleStateChanged(touch_handle_state);
}

void CefTouchHandleDrawableOSR::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  if (orientation == orientation_) {
    return;
  }

  orientation_ = orientation;

  CefSize size;
  auto browser = rwhv_->browser_impl();
  auto handler = browser->GetClient()->GetRenderHandler();
  handler->GetTouchHandleSize(
      browser.get(), static_cast<cef_horizontal_alignment_t>(orientation_),
      size);

  const gfx::Size& image_size = gfx::Size(size.width, size.height);
  int handle_width = image_size.width() + 2 * kSelectionHandlePadding;
  int handle_height = image_size.height() + 2 * kSelectionHandlePadding;
  relative_bounds_ =
      gfx::RectF(-kSelectionHandlePadding,
                 kSelectionHandleVerticalVisualOffset - kSelectionHandlePadding,
                 handle_width, handle_height);

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ORIENTATION;
  touch_handle_state.orientation =
      static_cast<cef_horizontal_alignment_t>(orientation_);
  touch_handle_state.mirror_vertical = mirror_vertical;
  touch_handle_state.mirror_horizontal = mirror_horizontal;
  TouchHandleStateChanged(touch_handle_state);
}

void CefTouchHandleDrawableOSR::SetOrigin(const gfx::PointF& position) {
  if (position == origin_position_) {
    return;
  }

  origin_position_ = position;

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ORIGIN;
  touch_handle_state.origin = {static_cast<int>(std::round(position.x())),
                               static_cast<int>(std::round(position.y()))};
  TouchHandleStateChanged(touch_handle_state);
}

void CefTouchHandleDrawableOSR::SetAlpha(float alpha) {
  if (alpha == alpha_) {
    return;
  }

  alpha_ = alpha;

  CefTouchHandleState touch_handle_state;
  touch_handle_state.touch_handle_id = id_;
  touch_handle_state.flags = CEF_THS_FLAG_ALPHA;
  touch_handle_state.alpha = alpha_;
  TouchHandleStateChanged(touch_handle_state);
}

gfx::RectF CefTouchHandleDrawableOSR::GetVisibleBounds() const {
  gfx::RectF bounds = relative_bounds_;
  bounds.Offset(origin_position_.x(), origin_position_.y());
  bounds.Inset(gfx::InsetsF::TLBR(
      kSelectionHandlePadding,
      kSelectionHandlePadding + kSelectionHandleVerticalVisualOffset,
      kSelectionHandlePadding, kSelectionHandlePadding));
  return bounds;
}

float CefTouchHandleDrawableOSR::GetDrawableHorizontalPaddingRatio() const {
  return 0.0f;
}

void CefTouchHandleDrawableOSR::TouchHandleStateChanged(
    const CefTouchHandleState& state) {
  auto browser = rwhv_->browser_impl();
  auto handler = browser->GetClient()->GetRenderHandler();
  handler->OnTouchHandleStateChanged(browser.get(), state);
}
