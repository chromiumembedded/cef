// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_LINUX_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_LINUX_H_
#pragma once

#include "tests/shared/browser/osr_touchpad.h"

namespace client {

// Reads type-B evdev touchpads independently of the GTK display backend. The
// caller watches fd() on its main loop and calls Read() when input is ready.
// No exclusive grab is taken; conflicting GTK input is suppressed by the view.
class OsrTouchpadLinux {
 public:
  explicit OsrTouchpadLinux(OsrTouchpadCallback callback);
  ~OsrTouchpadLinux();

  OsrTouchpadLinux(const OsrTouchpadLinux&) = delete;
  OsrTouchpadLinux& operator=(const OsrTouchpadLinux&) = delete;

  bool Open();
  int fd() const { return fd_; }
  bool Read();

 private:
  struct Slot {
    int id = -1;
    int x = 0;
    int y = 0;
  };

  bool ReadSnapshot();
  bool DiscardQueued();
  void EmitFrame();

  const OsrTouchpadCallback callback_;
  int fd_ = -1;
  int slot_ = 0;
  int min_x_ = 0;
  int max_x_ = 0;
  int min_y_ = 0;
  int max_y_ = 0;
  std::vector<Slot> slots_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_OSR_TOUCHPAD_LINUX_H_
