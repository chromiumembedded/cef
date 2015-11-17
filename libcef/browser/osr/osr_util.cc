// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/osr_util.h"

namespace osr_util {

namespace {

// The rate at which new calls to OnPaint will be generated.
const int kDefaultFrameRate = 30;
const int kMaximumFrameRate = 60;

}  // namespace

int ClampFrameRate(int frame_rate) {
  if (frame_rate < 1)
    return kDefaultFrameRate;
  else if (frame_rate > kMaximumFrameRate)
    return kMaximumFrameRate;
  return frame_rate;
}

}  // namespace osr_util
