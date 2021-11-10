// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_TASK_RUNNER_MANAGER_H_
#define CEF_LIBCEF_COMMON_TASK_RUNNER_MANAGER_H_
#pragma once

#include "base/task/single_thread_task_runner.h"

// Exposes global sequenced task runners in the main and render processes.
// Prefer using base::ThreadPool for tasks that do not need to be globally
// sequenced and CefTaskRunner for retrieving named CefThreadId runners.
class CefTaskRunnerManager {
 public:
  // Returns the singleton instance that is scoped to CEF lifespan.
  static CefTaskRunnerManager* Get();

  // Available in the main process:
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetBackgroundTaskRunner() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetUserVisibleTaskRunner() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetUserBlockingTaskRunner() = 0;

  // Available in the render process:
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetRenderTaskRunner() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetWebWorkerTaskRunner() = 0;

 protected:
  CefTaskRunnerManager();
  virtual ~CefTaskRunnerManager();
};

#endif  // CEF_LIBCEF_COMMON_TASK_RUNNER_MANAGER_H_
