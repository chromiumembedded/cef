// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/task_runner_impl.h"
#include "libcef/common/task_runner_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"

using content::BrowserThread;

// CefTaskRunner

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForCurrentThread() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      CefTaskRunnerImpl::GetCurrentTaskRunner();
  if (task_runner.get())
    return new CefTaskRunnerImpl(task_runner);
  return nullptr;
}

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForThread(CefThreadId threadId) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get())
    return new CefTaskRunnerImpl(task_runner);

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
  if (!manager)
    return nullptr;

  int id = -1;
  switch (threadId) {
    case TID_UI:
      id = BrowserThread::UI;
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
      id = BrowserThread::IO;
      break;
    case TID_RENDERER:
      return manager->GetRenderTaskRunner();
    default:
      break;
  };

  if (id >= 0 &&
      BrowserThread::IsThreadInitialized(static_cast<BrowserThread::ID>(id))) {
    // Specify USER_BLOCKING so that BrowserTaskExecutor::GetTaskRunner always
    // gives us the same TaskRunner object.
    return base::CreateSingleThreadTaskRunner(
        {static_cast<BrowserThread::ID>(id),
         base::TaskPriority::USER_BLOCKING});
  }

  return nullptr;
}

// static
scoped_refptr<base::SingleThreadTaskRunner>
CefTaskRunnerImpl::GetCurrentTaskRunner() {
  auto* manager = CefTaskRunnerManager::Get();
  if (!manager)
    return nullptr;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner;

  // For named browser process threads return the same TaskRunner as
  // GetTaskRunner(). Otherwise BelongsToThread() will return incorrect results.
  BrowserThread::ID current_id;
  if (BrowserThread::GetCurrentThreadIdentifier(&current_id) &&
      BrowserThread::IsThreadInitialized(current_id)) {
    // Specify USER_BLOCKING so that BrowserTaskExecutor::GetTaskRunner always
    // gives us the same TaskRunner object.
    task_runner = base::CreateSingleThreadTaskRunner(
        {current_id, base::TaskPriority::USER_BLOCKING});
  }

  if (!task_runner.get()) {
    // Check for a MessageLoopProxy. This covers all of the named browser and
    // render process threads, plus a few extra.
    task_runner = base::ThreadTaskRunnerHandle::Get();
  }

  if (!task_runner.get()) {
    // Check for a WebWorker thread.
    return manager->GetWebWorkerTaskRunner();
  }

  return task_runner;
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
                                        int64 delay_ms) {
  return task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&CefTask::Execute, task.get()),
      base::Milliseconds(delay_ms));
}
