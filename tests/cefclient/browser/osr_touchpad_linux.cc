// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "tests/cefclient/browser/osr_touchpad_linux.h"

#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <utility>

namespace client {

namespace {

template <size_t N>
bool HasBit(const std::array<uint8_t, N>& bits, unsigned int bit) {
  return bit / 8 < N && (bits[bit / 8] & (1U << (bit % 8)));
}

}  // namespace

OsrTouchpadLinux::OsrTouchpadLinux(OsrTouchpadCallback callback)
    : callback_(std::move(callback)) {}

OsrTouchpadLinux::~OsrTouchpadLinux() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

bool OsrTouchpadLinux::Open() {
  glob_t paths = {};
  glob("/dev/input/event*", 0, nullptr, &paths);
  for (size_t i = 0; i < paths.gl_pathc; ++i) {
    fd_ = open(paths.gl_pathv[i], O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
      continue;
    }
    std::array<uint8_t, (INPUT_PROP_MAX + 8) / 8> props = {};
    std::array<uint8_t, (KEY_MAX + 8) / 8> keys = {};
    input_absinfo slots = {}, x = {}, y = {};
    if (ioctl(fd_, EVIOCGPROP(props.size()), props.data()) >= 0 &&
        ioctl(fd_, EVIOCGBIT(EV_KEY, keys.size()), keys.data()) >= 0 &&
        HasBit(props, INPUT_PROP_POINTER) &&
        !HasBit(props, INPUT_PROP_DIRECT) &&
        !HasBit(props, INPUT_PROP_SEMI_MT) && HasBit(keys, BTN_TOOL_FINGER) &&
        !HasBit(keys, BTN_TOOL_PEN) &&
        ioctl(fd_, EVIOCGABS(ABS_MT_SLOT), &slots) >= 0 &&
        ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_X), &x) >= 0 &&
        ioctl(fd_, EVIOCGABS(ABS_MT_POSITION_Y), &y) >= 0 &&
        slots.minimum == 0 && slots.maximum > 0 && slots.maximum < 256 &&
        x.maximum > x.minimum && y.maximum > y.minimum) {
      slots_.assign(slots.maximum + 1, {});
      min_x_ = x.minimum;
      max_x_ = x.maximum;
      min_y_ = y.minimum;
      max_y_ = y.maximum;
      if (DiscardQueued() && ReadSnapshot()) {
        globfree(&paths);
        EmitFrame();
        return true;
      }
    }
    close(fd_);
    fd_ = -1;
  }
  globfree(&paths);
  return false;
}

bool OsrTouchpadLinux::DiscardQueued() {
  input_event events[64];
  for (;;) {
    const ssize_t bytes = read(fd_, events, sizeof(events));
    if (bytes > 0 || (bytes < 0 && errno == EINTR)) {
      continue;
    }
    return bytes < 0 && errno == EAGAIN;
  }
}

bool OsrTouchpadLinux::ReadSnapshot() {
  input_absinfo current = {};
  if (ioctl(fd_, EVIOCGABS(ABS_MT_SLOT), &current) < 0 || current.value < 0 ||
      static_cast<size_t>(current.value) >= slots_.size()) {
    return false;
  }
  slot_ = current.value;
  std::vector<int32_t> values(slots_.size() + 1);
  for (const auto code :
       {ABS_MT_TRACKING_ID, ABS_MT_POSITION_X, ABS_MT_POSITION_Y}) {
    values[0] = code;
    if (ioctl(fd_, EVIOCGMTSLOTS(values.size() * sizeof(int32_t)),
              values.data()) < 0) {
      return false;
    }
    for (size_t i = 0; i < slots_.size(); ++i) {
      if (code == ABS_MT_TRACKING_ID) {
        slots_[i].id = values[i + 1];
      } else if (code == ABS_MT_POSITION_X) {
        slots_[i].x = values[i + 1];
      } else {
        slots_[i].y = values[i + 1];
      }
    }
  }
  return true;
}

void OsrTouchpadLinux::EmitFrame() {
  OsrTouchpadFrame frame;
  for (const auto& slot : slots_) {
    if (slot.id >= 0) {
      frame.push_back({static_cast<uint64_t>(slot.id),
                       NormalizeTouchpadAxis(slot.x, min_x_, max_x_),
                       NormalizeTouchpadAxis(slot.y, min_y_, max_y_), true});
    }
  }
  callback_(std::move(frame));
}

bool OsrTouchpadLinux::Read() {
  input_event events[64];
  for (;;) {
    const ssize_t bytes = read(fd_, events, sizeof(events));
    if (bytes < 0 && errno == EINTR) {
      continue;
    }
    if (bytes < 0 && errno == EAGAIN) {
      return true;
    }
    if (bytes <= 0 || bytes % sizeof(input_event)) {
      callback_(std::nullopt);
      return false;
    }
    for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event);
         ++i) {
      const auto& event = events[i];
      if (event.type == EV_SYN && event.code == SYN_DROPPED) {
        callback_(std::nullopt);
        // Discard the remainder of this batch and all queued input before
        // querying current slots. Never replay stale events over that snapshot.
        if (!DiscardQueued() || !ReadSnapshot()) {
          return false;
        }
        EmitFrame();
        return true;
      }
      if (event.type == EV_ABS) {
        if (event.code == ABS_MT_SLOT) {
          slot_ = event.value;
        } else if (slot_ >= 0 && static_cast<size_t>(slot_) < slots_.size()) {
          auto& slot = slots_[slot_];
          if (event.code == ABS_MT_TRACKING_ID) {
            slot.id = event.value;
          } else if (event.code == ABS_MT_POSITION_X) {
            slot.x = event.value;
          } else if (event.code == ABS_MT_POSITION_Y) {
            slot.y = event.value;
          }
        }
      } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
        EmitFrame();
      }
    }
  }
}

}  // namespace client
