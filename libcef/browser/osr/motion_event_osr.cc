// Copyright (c) 2017 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/motion_event_osr.h"

#include <algorithm>

#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace {

ui::MotionEvent::ToolType CefPointerTypeToMotionEventToolType(
    cef_pointer_type_t pointer_type) {
  switch (pointer_type) {
    case CEF_POINTER_TYPE_TOUCH:
      return ui::MotionEvent::ToolType::FINGER;
    case CEF_POINTER_TYPE_MOUSE:
      return ui::MotionEvent::ToolType::MOUSE;
    case CEF_POINTER_TYPE_PEN:
      return ui::MotionEvent::ToolType::STYLUS;
    case CEF_POINTER_TYPE_ERASER:
      return ui::MotionEvent::ToolType::ERASER;
    case CEF_POINTER_TYPE_UNKNOWN:
      return ui::MotionEvent::ToolType::UNKNOWN;
  }
  DCHECK(false);
  return ui::MotionEvent::ToolType::UNKNOWN;
}

}  // namespace

CefMotionEventOSR::CefMotionEventOSR() {
  std::fill(id_map_, id_map_ + blink::WebTouchEvent::kTouchesLengthCap, -1);
}

CefMotionEventOSR::~CefMotionEventOSR() = default;

int CefMotionEventOSR::GetSourceDeviceId(size_t pointer_index) const {
  if (IsValidIndex(pointer_index)) {
    return pointer(pointer_index).source_device_id;
  } else {
    return -1;
  }
}

// Returns true if the touch was valid.
bool CefMotionEventOSR::OnTouch(const CefTouchEvent& touch) {
  int id = LookupId(touch.id);
  bool pointer_id_is_active = id != -1;

  if (touch.type == CEF_TET_PRESSED && pointer_id_is_active) {
    // Ignore pressed events for already active touches.
    return false;
  } else if (touch.type != CEF_TET_PRESSED && !pointer_id_is_active) {
    // When a window begins capturing touch events, we could have an active
    // touch stream transfered to us, resulting in touch move or touch up
    // events without associated touch down events. Ignore them.
    return false;
  }

  switch (touch.type) {
    case CEF_TET_PRESSED:
      id = AddId(touch.id);
      if (id == -1) {
        return false;
      }
      if (!AddTouch(touch, id)) {
        return false;
      }
      break;

    case CEF_TET_MOVED: {
      // Discard if touch is stationary.
      int index = FindPointerIndexOfId(id);
      if (IsValidIndex(index) &&
          (touch.x == GetX(index) && touch.y == GetY(index))) {
        return false;
      }
    }
      [[fallthrough]];
    // No break.
    case CEF_TET_RELEASED:
    case CEF_TET_CANCELLED:
      // Removing these touch points needs to be postponed until after the
      // MotionEvent has been dispatched. This cleanup occurs in
      // CleanupRemovedTouchPoints.
      UpdateTouch(touch, id);
      break;
  }

  UpdateCachedAction(touch, id);
  set_unique_event_id(ui::GetNextTouchEventId());
  set_flags(touch.modifiers);
  set_event_time(base::TimeTicks::Now());
  return true;
}

// We can't cleanup removed touch points immediately upon receipt of a
// TouchCancel or TouchRelease, as the MotionEvent needs to be able to report
// information about those touch events. Once the MotionEvent has been
// processed, we call CleanupRemovedTouchPoints to do the required
// book-keeping.
void CefMotionEventOSR::CleanupRemovedTouchPoints(const CefTouchEvent& event) {
  if (event.type != CEF_TET_RELEASED && event.type != CEF_TET_CANCELLED) {
    return;
  }

  DCHECK(GetPointerCount());

  int id = LookupId(event.id);
  int index_to_delete = FindPointerIndexOfId(id);
  set_action_index(-1);
  set_action(ui::MotionEvent::Action::NONE);
  if (IsValidIndex(index_to_delete)) {
    pointer(index_to_delete) = pointer(GetPointerCount() - 1);
    PopPointer();
    RemoveId(event.id);
  }
}

// Reset unchanged touch point to StateStationary for touchmove and touchcancel.
void CefMotionEventOSR::MarkUnchangedTouchPointsAsStationary(
    blink::WebTouchEvent* event,
    const CefTouchEvent& cef_event) {
  int id = LookupId(cef_event.id);
  if (event->GetType() == blink::WebInputEvent::Type::kTouchMove ||
      event->GetType() == blink::WebInputEvent::Type::kTouchCancel) {
    for (size_t i = 0; i < event->touches_length; ++i) {
      if (event->touches[i].id != id) {
        event->touches[i].state = blink::WebTouchPoint::State::kStateStationary;
      }
    }
  }
}

int CefMotionEventOSR::LookupId(int id) {
  if (id == -1) {
    return -1;
  }

  for (int i = 0; i < blink::WebTouchEvent::kTouchesLengthCap; i++) {
    if (id_map_[i] == id) {
      return i;
    }
  }
  return -1;
}

int CefMotionEventOSR::AddId(int id) {
  if (id == -1 || LookupId(id) >= 0) {
    return -1;
  }

  for (int i = 0; i < blink::WebTouchEvent::kTouchesLengthCap; i++) {
    if (id_map_[i] == -1) {
      id_map_[i] = id;
      return i;
    }
  }

  return -1;
}

void CefMotionEventOSR::RemoveId(int id) {
  for (int& i : id_map_) {
    if (i == id) {
      i = -1;
    }
  }
}

bool CefMotionEventOSR::AddTouch(const CefTouchEvent& touch, int id) {
  if (GetPointerCount() == MotionEvent::MAX_TOUCH_POINT_COUNT) {
    return false;
  }

  PushPointer(GetPointerPropertiesFromTouchEvent(touch, id));
  return true;
}

void CefMotionEventOSR::UpdateTouch(const CefTouchEvent& touch, int id) {
  int index_to_update = FindPointerIndexOfId(id);
  if (IsValidIndex(index_to_update)) {
    pointer(index_to_update) = GetPointerPropertiesFromTouchEvent(touch, id);
  }
}

void CefMotionEventOSR::UpdateCachedAction(const CefTouchEvent& touch, int id) {
  DCHECK(GetPointerCount());
  switch (touch.type) {
    case CEF_TET_PRESSED:
      if (GetPointerCount() == 1) {
        set_action(ui::MotionEvent::Action::DOWN);
      } else {
        set_action(ui::MotionEvent::Action::POINTER_DOWN);
        set_action_index(FindPointerIndexOfId(id));
      }
      break;
    case CEF_TET_RELEASED:
      if (GetPointerCount() == 1) {
        set_action(ui::MotionEvent::Action::UP);
      } else {
        set_action(ui::MotionEvent::Action::POINTER_UP);
        set_action_index(FindPointerIndexOfId(id));
      }
      break;
    case CEF_TET_CANCELLED:
      set_action(ui::MotionEvent::Action::CANCEL);
      break;
    case CEF_TET_MOVED:
      set_action(ui::MotionEvent::Action::MOVE);
      break;
  }
}

bool CefMotionEventOSR::IsValidIndex(int index) const {
  return (index >= 0) && (index < static_cast<int>(GetPointerCount()));
}

ui::PointerProperties CefMotionEventOSR::GetPointerPropertiesFromTouchEvent(
    const CefTouchEvent& touch,
    int id) {
  ui::PointerProperties pointer_properties;
  pointer_properties.x = touch.x;
  pointer_properties.y = touch.y;
  pointer_properties.raw_x = touch.x;
  pointer_properties.raw_y = touch.y;
  pointer_properties.id = id;
  pointer_properties.pressure = touch.pressure;
  pointer_properties.source_device_id = 0;

  pointer_properties.SetAxesAndOrientation(touch.radius_x, touch.radius_y,
                                           touch.rotation_angle);
  if (!pointer_properties.touch_major) {
    float default_size;
    switch (touch.pointer_type) {
      case CEF_POINTER_TYPE_PEN:
      case CEF_POINTER_TYPE_ERASER:
        // Default size for stylus events is 1x1.
        default_size = 1;
        break;
      default:
        default_size =
            2.f * ui::GestureConfiguration::GetInstance()->default_radius();
        break;
    }
    pointer_properties.touch_major = pointer_properties.touch_minor =
        default_size;
    pointer_properties.orientation = 0;
  }

  pointer_properties.tool_type =
      CefPointerTypeToMotionEventToolType(touch.pointer_type);

  return pointer_properties;
}
