// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_WAITABLE_EVENT_IMPL_H_
#define CEF_LIBCEF_COMMON_WAITABLE_EVENT_IMPL_H_
#pragma once

#include "include/cef_waitable_event.h"

#include "base/synchronization/waitable_event.h"

class CefWaitableEventImpl : public CefWaitableEvent {
 public:
  CefWaitableEventImpl(bool automatic_reset, bool initially_signaled);

  CefWaitableEventImpl(const CefWaitableEventImpl&) = delete;
  CefWaitableEventImpl& operator=(const CefWaitableEventImpl&) = delete;

  // CefWaitableEvent methods:
  void Reset() override;
  void Signal() override;
  bool IsSignaled() override;
  void Wait() override;
  bool TimedWait(int64_t max_ms) override;

 private:
  base::WaitableEvent event_;

  IMPLEMENT_REFCOUNTING(CefWaitableEventImpl);
};

#endif  // CEF_LIBCEF_COMMON_WAITABLE_EVENT_IMPL_H_
