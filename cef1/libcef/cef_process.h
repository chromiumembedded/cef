// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is for managing the global services of the application. Each
// service is lazily created when requested the first time. The service getters
// will return NULL if the service is not available, so callers must check for
// this condition.

#ifndef CEF_LIBCEF_CEF_PROCESS_H_
#define CEF_LIBCEF_CEF_PROCESS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_message.h"

namespace base {
class Thread;
class WaitableEvent;
}

class CefProcessIOThread;
class CefProcessUIThread;
class CefMessageLoopForUI;

// NOT THREAD SAFE, call only from the main thread.
// These functions shouldn't return NULL unless otherwise noted.
class CefProcess : public base::NonThreadSafe {
 public:
  explicit CefProcess(bool multi_threaded_message_loop);
  virtual ~CefProcess();

  // Creates key child threads. We need to do this explicitly since
  // CefThread::PostTask silently deletes a posted task if the target message
  // loop isn't created.
  void CreateChildThreads() {
    ui_thread();
    // Create the FILE thread before the IO thread because IO thread
    // initialization depends on existance of the FILE thread (for cache
    // support, etc).
    file_thread();
    io_thread();
  }

  CefProcessUIThread* ui_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_ui_thread_)
      CreateUIThread();
    return ui_thread_.get();
  }

  // Do a single iteration of the UI message loop on the current thread. If
  // RunMessageLoop() was called you do not need to call this method.
  void DoMessageLoopIteration();

  // Run the UI message loop on the current thread.
  void RunMessageLoop();

  // Quit the UI message loop on the current thread.
  void QuitMessageLoop();

  // Returns the thread that we perform I/O coordination on (network requests,
  // communication with renderers, etc.
  // NOTE: You should ONLY use this to pass to IPC or other objects which must
  // need a MessageLoop*.  If you just want to post a task, use
  // CefThread::PostTask (or other variants) as they take care of checking
  // that a thread is still alive, race conditions, lifetime differences etc.
  // If you still must use this, need to check the return value for NULL.
  CefProcessIOThread* io_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_io_thread_)
      CreateIOThread();
    return io_thread_.get();
  }

  // Returns the thread that we perform random file operations on. For code
  // that wants to do I/O operations (not network requests or even file: URL
  // requests), this is the thread to use to avoid blocking the UI thread.
  // It might be nicer to have a thread pool for this kind of thing.
  base::Thread* file_thread() {
    DCHECK(CalledOnValidThread());
    if (!created_file_thread_)
      CreateFileThread();
    return file_thread_.get();
  }

#if defined(IPC_MESSAGE_LOG_ENABLED)
  // Enable or disable IPC logging for the browser, all processes
  // derived from ChildProcess (plugin etc), and all
  // renderers.
  void SetIPCLoggingEnabled(bool enable);
#endif

 private:
  void CreateUIThread();
  void CreateIOThread();
  void CreateFileThread();

  bool multi_threaded_message_loop_;

  bool created_ui_thread_;
  scoped_ptr<CefProcessUIThread> ui_thread_;
  scoped_ptr<CefMessageLoopForUI> ui_message_loop_;

  bool created_io_thread_;
  scoped_ptr<CefProcessIOThread> io_thread_;

  bool created_file_thread_;
  scoped_ptr<base::Thread> file_thread_;

  DISALLOW_COPY_AND_ASSIGN(CefProcess);
};

#endif  // CEF_LIBCEF_CEF_PROCESS_H_
