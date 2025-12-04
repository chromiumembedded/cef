// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/common/waitable_event_impl.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "cef/libcef/common/task_util.h"

namespace {

bool AllowWait() {
  // Use internal variant that doesn't log before CefInitialize.
  if (cef::CurrentlyOnThread(TID_UI) || cef::CurrentlyOnThread(TID_IO)) {
    DCHECK(false) << "waiting is not allowed on the current thread";
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
    : event_(automatic_reset ? base::WaitableEvent::ResetPolicy::AUTOMATIC
                             : base::WaitableEvent::ResetPolicy::MANUAL,
             initially_signaled
                 ? base::WaitableEvent::InitialState::SIGNALED
                 : base::WaitableEvent::InitialState::NOT_SIGNALED) {}

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
  if (!AllowWait()) {
    return;
  }
  event_.Wait();
}

bool CefWaitableEventImpl::TimedWait(int64_t max_ms) {
  if (!AllowWait()) {
    return false;
  }
  return event_.TimedWait(base::Milliseconds(max_ms));
}
