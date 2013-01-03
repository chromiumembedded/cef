// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/task_runner_impl.h"
#include "libcef/renderer/content_renderer_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

// CefTaskRunner

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForCurrentThread() {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetCurrentTaskRunner();
  if (task_runner.get())
    return new CefTaskRunnerImpl(task_runner);
  return NULL;
}

// static
CefRefPtr<CefTaskRunner> CefTaskRunner::GetForThread(CefThreadId threadId) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      CefTaskRunnerImpl::GetTaskRunner(threadId);
  if (task_runner.get())
    return new CefTaskRunnerImpl(task_runner);

  DLOG(WARNING) << "Invalid thread id " << threadId;
  return NULL;
}


// CefTaskRunnerImpl

CefTaskRunnerImpl::CefTaskRunnerImpl(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_.get());
}

// static
scoped_refptr<base::SequencedTaskRunner>
    CefTaskRunnerImpl::GetTaskRunner(CefThreadId threadId) {
  // Render process.
  if (threadId == TID_RENDERER) {
    CefContentRendererClient* client = CefContentRendererClient::Get();
    if (client)
      return client->render_task_runner();
    return NULL;
  }

  // Browser process.
  int id = -1;
  switch (threadId) {
  case TID_UI:
    id = BrowserThread::UI;
    break;
  case TID_DB:
    id = BrowserThread::DB;
    break;
  case TID_FILE:
    id = BrowserThread::FILE;
    break;
  case TID_FILE_USER_BLOCKING:
    id = BrowserThread::FILE_USER_BLOCKING;
    break;
  case TID_PROCESS_LAUNCHER:
    id = BrowserThread::PROCESS_LAUNCHER;
    break;
  case TID_CACHE:
    id = BrowserThread::CACHE;
    break;
  case TID_IO:
    id = BrowserThread::IO;
    break;
  default:
    break;
  };

  if (id >= 0 && content::GetContentClient()->browser() &&
      BrowserThread::IsMessageLoopValid(static_cast<BrowserThread::ID>(id))) {
    // Don't use BrowserThread::GetMessageLoopProxyForThread because it returns
    // a new MessageLoopProxy object for each call and makes pointer equality
    // testing impossible.
    MessageLoop* message_loop =
        BrowserThread::UnsafeGetMessageLoopForThread(
            static_cast<BrowserThread::ID>(id));
    if (message_loop)
      return message_loop->message_loop_proxy();
  }

  return NULL;
}

// static
scoped_refptr<base::SequencedTaskRunner>
    CefTaskRunnerImpl::GetCurrentTaskRunner() {
  scoped_refptr<base::SequencedTaskRunner> task_runner;

  // Check for a MessageLoopProxy. This covers all of the named browser and
  // render process threads, plus a few extra.
  task_runner = base::MessageLoopProxy::current();

  if (!task_runner.get()) {
    // Check for a WebWorker thread.
    CefContentRendererClient* client = CefContentRendererClient::Get();
    if (client)
      task_runner = client->GetCurrentTaskRunner();
  }

  return task_runner;
}

bool CefTaskRunnerImpl::IsSame(CefRefPtr<CefTaskRunner> that) {
  CefTaskRunnerImpl* impl = static_cast<CefTaskRunnerImpl*>(that.get());
  return (impl && task_runner_ == impl->task_runner_);
}

bool CefTaskRunnerImpl::BelongsToCurrentThread() {
  return task_runner_->RunsTasksOnCurrentThread();
}

bool CefTaskRunnerImpl::BelongsToThread(CefThreadId threadId) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      GetTaskRunner(threadId);
  return (task_runner_ == task_runner);
}

bool CefTaskRunnerImpl::PostTask(CefRefPtr<CefTask> task) {
  return task_runner_->PostTask(FROM_HERE,
      base::Bind(&CefTask::Execute, task));
}

bool CefTaskRunnerImpl::PostDelayedTask(CefRefPtr<CefTask> task,
                                        int64 delay_ms) {
  return task_runner_->PostDelayedTask(FROM_HERE,
        base::Bind(&CefTask::Execute, task),
        base::TimeDelta::FromMilliseconds(delay_ms));
}
