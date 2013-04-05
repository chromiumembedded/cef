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
  virtual bool IsSame(CefRefPtr<CefTaskRunner> that) OVERRIDE;
  virtual bool BelongsToCurrentThread() OVERRIDE;
  virtual bool BelongsToThread(CefThreadId threadId) OVERRIDE;
  virtual bool PostTask(CefRefPtr<CefTask> task) OVERRIDE;
  virtual bool PostDelayedTask(CefRefPtr<CefTask> task,
                               int64 delay_ms) OVERRIDE;

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  IMPLEMENT_REFCOUNTING(CefTaskRunnerImpl);
  DISALLOW_COPY_AND_ASSIGN(CefTaskRunnerImpl);
};

#endif  // CEF_LIBCEF_COMMON_TASK_RUNNER_IMPL_H_
