// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/thread_helper.h"

#include <memory>

#include "include/wrapper/cef_closure_task.h"

void SignalEvent(CefRefPtr<CefWaitableEvent> event) {
  event->Signal();
}

void WaitForThread(CefThreadId thread_id, int64 delay_ms) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostDelayedTask(thread_id, base::BindOnce(SignalEvent, event), delay_ms);
  event->Wait();
}

void WaitForThread(CefRefPtr<CefTaskRunner> task_runner, int64 delay_ms) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  task_runner->PostDelayedTask(
      CefCreateClosureTask(base::BindOnce(SignalEvent, event)), delay_ms);
  event->Wait();
}

void RunOnThread(CefThreadId thread_id,
                 base::OnceClosure test_impl,
                 CefRefPtr<CefWaitableEvent> event) {
  if (!CefCurrentlyOn(thread_id)) {
    CefPostTask(thread_id, base::BindOnce(RunOnThread, thread_id,
                                          std::move(test_impl), event));
    return;
  }

  std::move(test_impl).Run();
  SignalEvent(event);
}

void RunOnThreadAsync(
    CefThreadId thread_id,
    base::OnceCallback<void(CefRefPtr<CefWaitableEvent>)> test_impl,
    CefRefPtr<CefWaitableEvent> event) {
  if (!CefCurrentlyOn(thread_id)) {
    CefPostTask(thread_id, base::BindOnce(RunOnThreadAsync, thread_id,
                                          std::move(test_impl), event));
    return;
  }

  std::move(test_impl).Run(event);
}
