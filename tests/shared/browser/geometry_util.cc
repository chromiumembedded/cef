// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/geometry_util.h"

#include <cmath>

namespace client {

int LogicalToDevice(int value, float device_scale_factor) {
  float scaled_val = static_cast<float>(value) * device_scale_factor;
  return static_cast<int>(std::floor(scaled_val));
}

CefRect LogicalToDevice(const CefRect& value, float device_scale_factor) {
  return CefRect(LogicalToDevice(value.x, device_scale_factor),
                 LogicalToDevice(value.y, device_scale_factor),
                 LogicalToDevice(value.width, device_scale_factor),
                 LogicalToDevice(value.height, device_scale_factor));
}

int DeviceToLogical(int value, float device_scale_factor) {
  float scaled_val = static_cast<float>(value) / device_scale_factor;
  return static_cast<int>(std::floor(scaled_val));
}

CefRect DeviceToLogical(const CefRect& value, float device_scale_factor) {
  return CefRect(DeviceToLogical(value.x, device_scale_factor),
                 DeviceToLogical(value.y, device_scale_factor),
                 DeviceToLogical(value.width, device_scale_factor),
                 DeviceToLogical(value.height, device_scale_factor));
}

void DeviceToLogical(CefMouseEvent& value, float device_scale_factor) {
  value.x = DeviceToLogical(value.x, device_scale_factor);
  value.y = DeviceToLogical(value.y, device_scale_factor);
}

void DeviceToLogical(CefTouchEvent& value, float device_scale_factor) {
  value.x = DeviceToLogical(value.x, device_scale_factor);
  value.y = DeviceToLogical(value.y, device_scale_factor);
}

void ConstrainWindowBounds(const CefRect& display, CefRect& window) {
  if (window.x < display.x) {
    window.x = display.x;
  }
  if (window.y < display.y) {
    window.y = display.y;
  }
  window.width = std::clamp(window.width, 100, display.width);
  window.height = std::clamp(window.height, 100, display.height);
  if (window.x + window.width >= display.x + display.width) {
    window.x = display.x + display.width - window.width;
  }
  if (window.y + window.height >= display.y + display.height) {
    window.y = display.y + display.height - window.height;
  }
}

}  // namespace client
