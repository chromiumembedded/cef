// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/software_output_device_osr.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"
#include "libcef/browser/thread_util.h"

#include "third_party/skia/src/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/skia_util.h"

CefSoftwareOutputDeviceOSR::CefSoftwareOutputDeviceOSR(
    ui::Compositor* compositor,
    bool transparent,
    const OnPaintCallback& callback)
    : transparent_(transparent),
      callback_(callback),
      active_(false) {
  CEF_REQUIRE_UIT();
  DCHECK(!callback_.is_null());
}

CefSoftwareOutputDeviceOSR::~CefSoftwareOutputDeviceOSR() {
  CEF_REQUIRE_UIT();
}

void CefSoftwareOutputDeviceOSR::Resize(const gfx::Size& viewport_pixel_size,
                                        float scale_factor) {
  CEF_REQUIRE_UIT();

  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;

  canvas_.reset(NULL);
  bitmap_.reset(new SkBitmap);
  bitmap_->allocN32Pixels(viewport_pixel_size.width(),
                          viewport_pixel_size.height(),
                          !transparent_);
  if (bitmap_->drawsNothing()) {
    NOTREACHED();
    bitmap_.reset(NULL);
    return;
  }

  if (transparent_)
    bitmap_->eraseARGB(0, 0, 0, 0);

  canvas_.reset(new SkCanvas(*bitmap_.get()));
}

SkCanvas* CefSoftwareOutputDeviceOSR::BeginPaint(const gfx::Rect& damage_rect) {
  CEF_REQUIRE_UIT();
  DCHECK(canvas_.get());
  DCHECK(bitmap_.get());

  damage_rect_ = damage_rect;

  return canvas_.get();
}

void CefSoftwareOutputDeviceOSR::EndPaint() {
  CEF_REQUIRE_UIT();
  DCHECK(canvas_.get());
  DCHECK(bitmap_.get());

  if (!bitmap_.get())
    return;

  cc::SoftwareOutputDevice::EndPaint();

  if (active_)
    OnPaint(damage_rect_);
}

void CefSoftwareOutputDeviceOSR::SetActive(bool active) {
  if (active == active_)
    return;
  active_ = active;

  // Call OnPaint immediately if deactivated while a damage rect is pending.
  if (!active_ && !pending_damage_rect_.IsEmpty())
    OnPaint(pending_damage_rect_);
}

void CefSoftwareOutputDeviceOSR::Invalidate(const gfx::Rect& damage_rect) {
  if (pending_damage_rect_.IsEmpty())
    pending_damage_rect_ = damage_rect;
  else
    pending_damage_rect_.Union(damage_rect);
}

void CefSoftwareOutputDeviceOSR::OnPaint(const gfx::Rect& damage_rect) {
  gfx::Rect rect = damage_rect;
  if (!pending_damage_rect_.IsEmpty()) {
    rect.Union(pending_damage_rect_);
    pending_damage_rect_.SetRect(0, 0, 0, 0);
  }

  rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (rect.IsEmpty())
    return;

  SkAutoLockPixels bitmap_pixels_lock(*bitmap_.get());
  callback_.Run(rect, bitmap_->width(), bitmap_->height(),
                bitmap_->getPixels());
}
