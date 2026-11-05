// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_OSR_TOUCHPAD_H_
#define CEF_TESTS_SHARED_BROWSER_OSR_TOUCHPAD_H_
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "tests/shared/browser/osr_input_util.h"

namespace client {

// Coordinates cover the touchpad's full surface, with a top-left origin.
struct OsrTouchpadContact {
  uint64_t id;
  float x;
  float y;
  bool down;
};
using OsrTouchpadFrame = std::vector<OsrTouchpadContact>;
// A missing frame signals lost synchronization or device removal.
using OsrTouchpadCallback =
    std::function<void(const std::optional<OsrTouchpadFrame>&)>;

float NormalizeTouchpadAxis(int64_t value, int64_t min, int64_t max);

// Match GetViewRect's rounding to whole DIPs at fractional device scales.
// Return pixels for popup-offset handling; the caller then converts to DIPs.
// Keep normalized edge contacts just inside the rendered viewport.
void MapTouchpadToView(CefTouchEvent& event,
                       int pixel_width,
                       int pixel_height,
                       float scale);

// Windows HID devices may split one scan across several reports. A zero
// contact count continues a pending frame, rather than releasing its fingers.
class OsrTouchpadFrameAssembler {
 public:
  std::optional<OsrTouchpadFrame> AddReport(size_t count,
                                            const OsrTouchpadFrame& report);

  void Reset();

 private:
  size_t expected_ = 0;
  OsrTouchpadFrame pending_;
};

// Produces normalized CefTouchEvents from complete native frames. The platform
// adapter maps these to view pixels, applies the latched popup offset, and
// converts to DIPs. This state is independent of the current API version.
class OsrTouchpadEmulator {
 public:
  bool enabled() const;

  std::vector<CefTouchEvent> SetEnabled(bool enabled);

  std::vector<CefTouchEvent> ProcessFrame(
      const std::optional<OsrTouchpadFrame>& frame);

 private:
  bool enabled_ = false;
  bool synchronized_ = true;
  bool waiting_for_release_ = false;
  std::set<uint64_t> previous_down_;
  std::map<uint64_t, CefTouchEvent> contacts_;
  OsrTouchInputState state_;
};

// Degraded single-contact input when raw touchpad contacts are unavailable.
// Coordinates and scroll deltas are normalized to the view, like touchpad
// contacts. Hover and orphan moves/releases do not start a contact.
constexpr int kPointerTouchScrollTimeoutMs = 150;

class OsrPointerTouchEmulator {
 public:
  bool enabled() const { return enabled_; }
  bool scrolling() const { return scrolling_; }
  std::vector<CefTouchEvent> SetEnabled(bool enabled);
  std::vector<CefTouchEvent> Pointer(cef_touch_event_type_t type,
                                     float x,
                                     float y);
  std::vector<CefTouchEvent> Scroll(float x, float y, float dx, float dy);
  // End a scrolling burst without generating a velocity-dependent fling.
  std::vector<CefTouchEvent> EndScroll();
  std::vector<CefTouchEvent> Cancel();

 private:
  bool enabled_ = false;
  bool active_ = false;
  bool scrolling_ = false;
  CefTouchEvent contact_;
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_OSR_TOUCHPAD_H_
