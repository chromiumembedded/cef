// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/waitable_event_impl.h"

#include "include/cef_task.h"

#include "base/time/time.h"

namespace {

bool AllowWait() {
  if (CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO)) {
    NOTREACHED() << "waiting is not allowed on the current thread";
    return false;
  }
  return true;
}

}  // namespace

// static
CefRefPtr<CefWaitableEvent> CefWaitableEvent::CreateWaitableEvent(
    bool automatic_reset,
    bool initially_signaled) {
  return new CefWaitableEventImpl(automatic_reset, initially_signaled);
}

CefWaitableEventImpl::CefWaitableEventImpl(bool automatic_reset,
                                           bool initially_signaled)
  : event_(automatic_reset ? base::WaitableEvent::ResetPolicy::AUTOMATIC :
                             base::WaitableEvent::ResetPolicy::MANUAL,
           initially_signaled ? base::WaitableEvent::InitialState::SIGNALED :
                              base::WaitableEvent::InitialState::NOT_SIGNALED) {
}

void CefWaitableEventImpl::Reset() {
  event_.Reset();
}

void CefWaitableEventImpl::Signal() {
  event_.Signal();
}

bool CefWaitableEventImpl::IsSignaled() {
  return event_.IsSignaled();
}

void CefWaitableEventImpl::Wait() {
  if (!AllowWait())
    return;
  event_.Wait();
}

bool CefWaitableEventImpl::TimedWait(int64 max_ms) {
  if (!AllowWait())
    return false;
  return event_.TimedWait(base::TimeDelta::FromMilliseconds(max_ms));
}
