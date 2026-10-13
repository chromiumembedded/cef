// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_REQUEST_STATE_H_
#define CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_REQUEST_STATE_H_
#pragma once

#include <utility>

#include "base/check.h"

// Serialize external BeginFrames until acknowledgement. Paint and
// acknowledgement use different Mojo pipes, so requests made from OnPaint
// can arrive while the preceding frame is still pending.
class CefExternalBeginFrameRequestState {
 public:
  // Returns true when the caller should issue a frame now. Overlapping
  // requests coalesce into one additional frame after acknowledgement.
  bool Request() {
    if (pending_) {
      queued_ = true;
      return false;
    }
    pending_ = true;
    return true;
  }

  // Completes the pending frame. Returns true if another frame was requested.
  bool Acknowledge() {
    DCHECK(pending_);
    pending_ = false;
    return std::exchange(queued_, false);
  }

  // No frame was issued because the compositor is unavailable.
  void Cancel() {
    pending_ = false;
    queued_ = false;
  }

  bool pending() const { return pending_; }

 private:
  bool pending_ = false;
  bool queued_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_REQUEST_STATE_H_
