// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/unittests/thread_helper.h"

#include "include/wrapper/cef_closure_task.h"

void SignalEvent(base::WaitableEvent* event) {
  event->Signal();
}

void WaitForThread(CefThreadId thread_id) {
  base::WaitableEvent event(true, false);
  CefPostTask(thread_id, base::Bind(SignalEvent, &event));
  event.Wait();
}

void WaitForThread(CefRefPtr<CefTaskRunner> task_runner) {
  base::WaitableEvent event(true, false);
  task_runner->PostTask(CefCreateClosureTask(base::Bind(SignalEvent, &event)));
  event.Wait();
}

void RunOnThread(CefThreadId thread_id,
                 const base::Callback<void(void)>& test_impl,
                 base::WaitableEvent* event) {
  if (!CefCurrentlyOn(thread_id)) {
    CefPostTask(thread_id,
        base::Bind(RunOnThread, thread_id, test_impl, event));
    return;
  }

  test_impl.Run();
  SignalEvent(event);
}

void RunOnThreadAsync(
    CefThreadId thread_id,
    const base::Callback<void(base::WaitableEvent*)>& test_impl,
    base::WaitableEvent* event) {
  if (!CefCurrentlyOn(thread_id)) {
    CefPostTask(thread_id,
        base::Bind(RunOnThreadAsync, thread_id, test_impl, event));
    return;
  }

  test_impl.Run(event);
}
