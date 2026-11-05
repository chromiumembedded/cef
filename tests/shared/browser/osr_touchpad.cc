// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/shared/browser/osr_touchpad.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "tests/shared/browser/geometry_util.h"

namespace client {

float NormalizeTouchpadAxis(int64_t value, int64_t min, int64_t max) {
  return max > min ? std::clamp((static_cast<double>(value) - min) /
                                    (static_cast<double>(max) - min),
                                0.0, 1.0)
                   : 0;
}

void MapTouchpadToView(CefTouchEvent& event,
                       int pixel_width,
                       int pixel_height,
                       float scale) {
  const float width = DeviceToLogical(pixel_width, scale);
  const float height = DeviceToLogical(pixel_height, scale);
  event.x = width > 0
                ? std::min(event.x * width, std::nextafter(width, 0.f)) * scale
                : 0;
  event.y =
      height > 0
          ? std::min(event.y * height, std::nextafter(height, 0.f)) * scale
          : 0;
}

std::optional<OsrTouchpadFrame> OsrTouchpadFrameAssembler::AddReport(
    size_t count,
    const OsrTouchpadFrame& report) {
  if (count > 256) {
    Reset();
    return std::nullopt;
  }
  if (count) {
    pending_.clear();
    expected_ = count;
  }
  if (!expected_) {
    return OsrTouchpadFrame();
  }
  const size_t size = std::min(report.size(), expected_ - pending_.size());
  pending_.insert(pending_.end(), report.begin(), report.begin() + size);
  if (pending_.size() != expected_) {
    return std::nullopt;
  }
  auto result = std::move(pending_);
  Reset();
  return result;
}

void OsrTouchpadFrameAssembler::Reset() {
  pending_.clear();
  expected_ = 0;
}

bool OsrTouchpadEmulator::enabled() const {
  return enabled_;
}

std::vector<CefTouchEvent> OsrTouchpadEmulator::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return {};
  }
  enabled_ = enabled;
  waiting_for_release_ = !synchronized_ || !previous_down_.empty();
  contacts_.clear();
  return state_.Cancel();
}

std::vector<CefTouchEvent> OsrTouchpadEmulator::ProcessFrame(
    const std::optional<OsrTouchpadFrame>& frame) {
  if (!frame) {
    synchronized_ = false;
    waiting_for_release_ = true;
    contacts_.clear();
    return state_.Cancel();
  }
  std::map<uint64_t, OsrTouchpadContact> current;
  std::set<uint64_t> down;
  for (const auto& point : *frame) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0 ||
        point.x > 1 || point.y < 0 || point.y > 1 ||
        !current.emplace(point.id, point).second) {
      return ProcessFrame(std::nullopt);
    }
    if (point.down) {
      down.insert(point.id);
    }
  }
  synchronized_ = true;
  if (!enabled_ || waiting_for_release_) {
    if (down.empty()) {
      waiting_for_release_ = false;
    }
    previous_down_ = std::move(down);
    return {};
  }

  std::vector<CefTouchEvent> events;
  // End old contacts first so their IDs can be reused within this frame.
  for (auto it = contacts_.begin(); it != contacts_.end();) {
    if (down.contains(it->first)) {
      ++it;
      continue;
    }
    auto event = it->second;
    if (auto point = current.find(it->first); point != current.end()) {
      event.x = point->second.x;
      event.y = point->second.y;
    }
    event.type = CEF_TET_RELEASED;
    state_.Update(it->first, event, false, 0, 0);
    events.push_back(event);
    it = contacts_.erase(it);
  }
  for (const auto& point : *frame) {
    if (!point.down) {
      continue;
    }
    auto it = contacts_.find(point.id);
    CefTouchEvent event;
    event.x = point.x;
    event.y = point.y;
    if (it != contacts_.end()) {
      if (event.x == it->second.x && event.y == it->second.y) {
        continue;
      }
      event.type = CEF_TET_MOVED;
    } else {
      // A contact ignored at capacity must lift before it can start again.
      if (previous_down_.contains(point.id)) {
        continue;
      }
      event.type = CEF_TET_PRESSED;
    }
    if (state_.Update(point.id, event, false, 0, 0)) {
      contacts_[point.id] = event;
      events.push_back(event);
    }
  }
  previous_down_ = std::move(down);
  return events;
}

std::vector<CefTouchEvent> OsrPointerTouchEmulator::SetEnabled(bool enabled) {
  enabled_ = enabled;
  return enabled ? std::vector<CefTouchEvent>() : Cancel();
}

std::vector<CefTouchEvent> OsrPointerTouchEmulator::Pointer(
    cef_touch_event_type_t type,
    float x,
    float y) {
  if (!enabled_ || !std::isfinite(x) || !std::isfinite(y) ||
      (type != CEF_TET_PRESSED && type != CEF_TET_MOVED &&
       type != CEF_TET_RELEASED) ||
      (type == CEF_TET_PRESSED ? active_ && !scrolling_
                               : !active_ || scrolling_)) {
    return {};
  }
  x = std::clamp(x, 0.f, 1.f);
  y = std::clamp(y, 0.f, 1.f);
  if (type == CEF_TET_MOVED && contact_.x == x && contact_.y == y) {
    return {};
  }
  // A new drag supersedes a wheel swipe. Hover and button-up messages during
  // scrolling must not move or release its contact.
  auto events = scrolling_ ? Cancel() : std::vector<CefTouchEvent>();
  contact_.id = 0;
  contact_.pointer_type = CEF_POINTER_TYPE_TOUCH;
  contact_.type = type;
  contact_.x = x;
  contact_.y = y;
  contact_.pressure = type == CEF_TET_RELEASED ? 0 : 0.5f;
  active_ = type != CEF_TET_RELEASED;
  events.push_back(contact_);
  return events;
}

std::vector<CefTouchEvent> OsrPointerTouchEmulator::Cancel() {
  if (!active_) {
    return {};
  }
  active_ = false;
  scrolling_ = false;
  contact_.type = CEF_TET_CANCELLED;
  contact_.pressure = 0;
  return {contact_};
}

std::vector<CefTouchEvent> OsrPointerTouchEmulator::Scroll(float x,
                                                           float y,
                                                           float dx,
                                                           float dy) {
  if (!enabled_ || (active_ && !scrolling_) || !std::isfinite(x) ||
      !std::isfinite(y) || !std::isfinite(dx) || !std::isfinite(dy) ||
      (!dx && !dy)) {
    return {};
  }
  // Bound unusually large deltas. Accumulate small deltas on one contact so
  // high-resolution scrolling can cross Chromium's touch slop threshold.
  dx = std::clamp(dx, -0.25f, 0.25f);
  dy = std::clamp(dy, -0.25f, 0.25f);
  std::vector<CefTouchEvent> events;
  if (!scrolling_) {
    x = std::clamp(x, std::max(0.f, -dx), std::min(1.f, 1.f - dx));
    y = std::clamp(y, std::max(0.f, -dy), std::min(1.f, 1.f - dy));
    events = Pointer(CEF_TET_PRESSED, x, y);
    scrolling_ = true;
  }
  x = std::clamp(contact_.x + dx, 0.f, 1.f);
  y = std::clamp(contact_.y + dy, 0.f, 1.f);
  if (x != contact_.x || y != contact_.y) {
    contact_.type = CEF_TET_MOVED;
    contact_.x = x;
    contact_.y = y;
    events.push_back(contact_);
  }
  return events;
}

std::vector<CefTouchEvent> OsrPointerTouchEmulator::EndScroll() {
  return scrolling_ ? Cancel() : std::vector<CefTouchEvent>();
}

}  // namespace client
