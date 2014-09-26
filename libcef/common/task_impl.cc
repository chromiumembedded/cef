// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_task.h"
#include "libcef/common/task_runner_impl.h"

#include "base/bind.h"
#include "base/location.h"

bool CefCurrentlyOn(CefThreadId threadId) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get())
    return task_runner->RunsTasksOnCurrentThread();
  return false;
}

bool CefPostTask(CefThreadId threadId, CefRefPtr<CefTask> task) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get()) {
    return task_runner->PostTask(FROM_HERE,
        base::Bind(&CefTask::Execute, task.get()));
  }
  return false;
}

bool CefPostDelayedTask(CefThreadId threadId, CefRefPtr<CefTask> task, int64 delay_ms) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get()) {
    return task_runner->PostDelayedTask(FROM_HERE,
        base::Bind(&CefTask::Execute, task.get()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }
  return false;
}
