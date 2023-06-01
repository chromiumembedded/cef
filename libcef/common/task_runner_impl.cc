// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/task_runner_impl.h"

#include "libcef/common/task_runner_manager.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"

using content::BrowserThread;

// CefTaskRunner

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForCurrentThread() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      CefTaskRunnerImpl::GetCurrentTaskRunner();
  if (task_runner.get()) {
    return new CefTaskRunnerImpl(task_runner);
  }
  return nullptr;
}

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForThread(CefThreadId threadId) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get()) {
    return new CefTaskRunnerImpl(task_runner);
  }

  LOG(WARNING) << "Invalid thread id " << threadId;
  return nullptr;
}

// CefTaskRunnerImpl

CefTaskRunnerImpl::CefTaskRunnerImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_.get());
}

// static
scoped_refptr<base::SingleThreadTaskRunner> CefTaskRunnerImpl::GetTaskRunner(
    CefThreadId threadId) {
  auto* manager = CefTaskRunnerManager::Get();
  if (!manager) {
    return nullptr;
  }

  switch (threadId) {
    case TID_UI:
      if (BrowserThread::IsThreadInitialized(BrowserThread::UI)) {
        return content::GetUIThreadTaskRunner({});
      }
      break;
    case TID_FILE_BACKGROUND:
      return manager->GetBackgroundTaskRunner();
    case TID_FILE_USER_VISIBLE:
      return manager->GetUserVisibleTaskRunner();
    case TID_FILE_USER_BLOCKING:
      return manager->GetUserBlockingTaskRunner();
    case TID_PROCESS_LAUNCHER:
      return content::GetProcessLauncherTaskRunner();
    case TID_IO:
      if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
        return content::GetIOThreadTaskRunner({});
      }
      break;
    case TID_RENDERER:
      return manager->GetRenderTaskRunner();
    default:
      break;
  };

  return nullptr;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
CefTaskRunnerImpl::GetCurrentTaskRunner() {
  auto* manager = CefTaskRunnerManager::Get();
  if (!manager) {
    return nullptr;
  }

  // For named browser process threads return the same TaskRunner as
  // GetTaskRunner(). Otherwise BelongsToThread() will return incorrect results.
  BrowserThread::ID current_id;
  if (BrowserThread::GetCurrentThreadIdentifier(&current_id) &&
      BrowserThread::IsThreadInitialized(current_id)) {
    if (current_id == BrowserThread::UI) {
      return content::GetUIThreadTaskRunner({});
    } else if (current_id == BrowserThread::IO) {
      return content::GetIOThreadTaskRunner({});
    } else {
      DCHECK(false);
    }
  }

  // Check for a MessageLoopProxy. This covers all of the named browser and
  // render process threads, plus a few extra.
  if (auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault()) {
    return task_runner;
  }

  // Check for a WebWorker thread.
  return manager->GetWebWorkerTaskRunner();
}

bool CefTaskRunnerImpl::IsSame(CefRefPtr<CefTaskRunner> that) {
  CefTaskRunnerImpl* impl = static_cast<CefTaskRunnerImpl*>(that.get());
  return (impl && task_runner_ == impl->task_runner_);
}

bool CefTaskRunnerImpl::BelongsToCurrentThread() {
  return task_runner_->RunsTasksInCurrentSequence();
}

bool CefTaskRunnerImpl::BelongsToThread(CefThreadId threadId) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner(threadId);
  return (task_runner_ == task_runner);
}

bool CefTaskRunnerImpl::PostTask(CefRefPtr<CefTask> task) {
  return task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&CefTask::Execute, task.get()));
}

bool CefTaskRunnerImpl::PostDelayedTask(CefRefPtr<CefTask> task,
                                        int64_t delay_ms) {
  return task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&CefTask::Execute, task.get()),
      base::Milliseconds(delay_ms));
}
