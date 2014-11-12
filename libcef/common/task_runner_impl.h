// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef  CEF_LIBCEF_COMMON_TASK_RUNNER_IMPL_H_
#define  CEF_LIBCEF_COMMON_TASK_RUNNER_IMPL_H_
#pragma once

#include "include/cef_task.h"
#include "base/sequenced_task_runner.h"

class CefTaskRunnerImpl : public CefTaskRunner {
 public:
  explicit CefTaskRunnerImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Returns the task runner associated with |threadId|.
  static scoped_refptr<base::SequencedTaskRunner>
      GetTaskRunner(CefThreadId threadId);
  // Returns the current task runner.
  static scoped_refptr<base::SequencedTaskRunner> GetCurrentTaskRunner();

  // CefTaskRunner methods:
  bool IsSame(CefRefPtr<CefTaskRunner> that) override;
  bool BelongsToCurrentThread() override;
  bool BelongsToThread(CefThreadId threadId) override;
  bool PostTask(CefRefPtr<CefTask> task) override;
  bool PostDelayedTask(CefRefPtr<CefTask> task,
                       int64 delay_ms) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  IMPLEMENT_REFCOUNTING(CefTaskRunnerImpl);
  DISALLOW_COPY_AND_ASSIGN(CefTaskRunnerImpl);
};

#endif  // CEF_LIBCEF_COMMON_TASK_RUNNER_IMPL_H_
