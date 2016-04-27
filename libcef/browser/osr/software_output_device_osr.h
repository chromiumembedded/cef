// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_OSR_H_

#include "base/callback.h"
#include "cc/output/software_output_device.h"

namespace ui {
class Compositor;
}

// Device implementation for direct software rendering via DelegatedFrameHost.
// All Rect/Size values are in pixels.
class CefSoftwareOutputDeviceOSR : public cc::SoftwareOutputDevice {
 public:
  typedef base::Callback<void(const gfx::Rect&,int,int,void*)>
      OnPaintCallback;

  CefSoftwareOutputDeviceOSR(ui::Compositor* compositor,
                             bool transparent,
                             const OnPaintCallback& callback);
  ~CefSoftwareOutputDeviceOSR() override;

  // cc::SoftwareOutputDevice implementation.
  void Resize(const gfx::Size& viewport_pixel_size,
             float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

  void SetActive(bool active);

  // Include |damage_rect| the next time OnPaint is called.
  void Invalidate(const gfx::Rect& damage_rect);

  // Deliver the OnPaint notification immediately.
  void OnPaint(const gfx::Rect& damage_rect);

 private:
  const bool transparent_;
  const OnPaintCallback callback_;

  bool active_;
  std::unique_ptr<SkCanvas> canvas_;
  std::unique_ptr<SkBitmap> bitmap_;
  gfx::Rect pending_damage_rect_;

  DISALLOW_COPY_AND_ASSIGN(CefSoftwareOutputDeviceOSR);
};

#endif  // CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_OSR_H_
