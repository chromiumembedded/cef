// Copyright 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/osr_render_handler_win.h"

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/shared/browser/util_win.h"

namespace client {

OsrRenderHandlerWin::OsrRenderHandlerWin(const OsrRendererSettings& settings,
                                         HWND hwnd)
    : settings_(settings),
      hwnd_(hwnd),
      begin_frame_pending_(false),
      weak_factory_(this) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(hwnd_);
}

OsrRenderHandlerWin::~OsrRenderHandlerWin() {
  CEF_REQUIRE_UI_THREAD();
}

void OsrRenderHandlerWin::SetBrowser(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  browser_ = browser;
  if (browser_ && settings_.external_begin_frame_enabled) {
    // Start the BeginFrame timer.
    Invalidate();
  }
}

void OsrRenderHandlerWin::Invalidate() {
  CEF_REQUIRE_UI_THREAD();
  if (begin_frame_pending_) {
    // The timer is already running.
    return;
  }

  // Trigger the BeginFrame timer.
  CHECK_GT(settings_.begin_frame_rate, 0);
  const float delay_us = (1.0 / double(settings_.begin_frame_rate)) * 1000000.0;
  TriggerBeginFrame(0, delay_us);
}

void OsrRenderHandlerWin::TriggerBeginFrame(uint64_t last_time_us,
                                            float delay_us) {
  if (begin_frame_pending_ && !settings_.external_begin_frame_enabled) {
    // Render immediately and then wait for the next call to Invalidate() or
    // On[Accelerated]Paint().
    begin_frame_pending_ = false;
    Render();
    return;
  }

  const auto now = GetTimeNow();
  float offset = now - last_time_us;
  if (offset > delay_us) {
    offset = delay_us;
  }

  if (!begin_frame_pending_) {
    begin_frame_pending_ = true;
  }

  // Trigger again after the necessary delay to maintain the desired frame rate.
  CefPostDelayedTask(TID_UI,
                     base::BindOnce(&OsrRenderHandlerWin::TriggerBeginFrame,
                                    weak_factory_.GetWeakPtr(), now, delay_us),
                     int64(offset / 1000.0));

  if (settings_.external_begin_frame_enabled && browser_) {
    // We're running the BeginFrame timer. Trigger rendering via
    // On[Accelerated]Paint().
    browser_->GetHost()->SendExternalBeginFrame();
  }
}

}  // namespace client
