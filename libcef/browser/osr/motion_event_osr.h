// Copyright (c) 2017 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_MOTION_EVENT_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_MOTION_EVENT_OSR_H_
#pragma once

#include "include/cef_base.h"

#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

// Implementation of MotionEvent which takes a stream of CefTouchEvents.
// This class is based on ui::MotionEventAura.
class CefMotionEventOSR : public ui::MotionEventGeneric {
 public:
  CefMotionEventOSR();

  CefMotionEventOSR(const CefMotionEventOSR&) = delete;
  CefMotionEventOSR& operator=(const CefMotionEventOSR&) = delete;

  ~CefMotionEventOSR() override;

  int GetSourceDeviceId(size_t pointer_index) const override;

  // Returns true if the touch was valid.
  bool OnTouch(const CefTouchEvent& touch);

  // We can't cleanup removed touch points immediately upon receipt of a
  // TouchCancel or TouchRelease, as the MotionEvent needs to be able to report
  // information about those touch events. Once the MotionEvent has been
  // processed, we call CleanupRemovedTouchPoints to do the required
  // book-keeping.
  void CleanupRemovedTouchPoints(const CefTouchEvent& event);

  // Reset unchanged touch point to StateStationary for touchmove and
  // touchcancel to make sure only send one ack per WebTouchEvent.
  void MarkUnchangedTouchPointsAsStationary(blink::WebTouchEvent* event,
                                            const CefTouchEvent& cef_event);

 private:
  // Chromium can't cope with touch ids >31, so let's map the incoming
  // ids to a safe range.
  int id_map_[blink::WebTouchEvent::kTouchesLengthCap];

  int LookupId(int id);
  int AddId(int id);
  void RemoveId(int id);

  bool AddTouch(const CefTouchEvent& touch, int id);
  void UpdateTouch(const CefTouchEvent& touch, int id);
  void UpdateCachedAction(const CefTouchEvent& touch, int id);
  bool IsValidIndex(int index) const;
  ui::PointerProperties GetPointerPropertiesFromTouchEvent(
      const CefTouchEvent& touch,
      int id);
};

#endif
