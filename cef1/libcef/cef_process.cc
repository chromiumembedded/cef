// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/cef_process.h"
#include "libcef/cef_process_io_thread.h"
#include "libcef/cef_process_sub_thread.h"
#include "libcef/cef_process_ui_thread.h"

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"

// Class used to process events on the current message loop.
class CefMessageLoopForUI : public MessageLoopForUI {
  typedef MessageLoopForUI inherited;

 public:
  CefMessageLoopForUI()
    : is_iterating_(true) {
  }
#if defined(OS_MACOSX)
  virtual ~CefMessageLoopForUI() {
    // On Mac the MessageLoop::AutoRunState scope in Run() never exits so clear
    // the run_loop_ variable to avoid an assertion in the MessageLoop
    // destructor.
    run_loop_ = NULL;
  }
#endif

  // Returns the MessageLoopForUI of the current thread.
  static CefMessageLoopForUI* current() {
    MessageLoop* loop = MessageLoop::current();
    DCHECK_EQ(MessageLoop::TYPE_UI, loop->type());
    return static_cast<CefMessageLoopForUI*>(loop);
  }

  virtual bool DoIdleWork() {
    bool valueToRet = inherited::DoIdleWork();
    if (is_iterating_)
      pump_->Quit();
    return valueToRet;
  }

  // Do a single interation of the UI message loop.
  void DoMessageLoopIteration() {
    Run();
  }

  // Run the UI message loop.
  void RunMessageLoop() {
    is_iterating_ = false;
    DoMessageLoopIteration();
  }

  bool is_iterating() { return is_iterating_; }

 private:
  // True if the message loop is doing one iteration at a time.
  bool is_iterating_;

  DISALLOW_COPY_AND_ASSIGN(CefMessageLoopForUI);
};

CefProcess::CefProcess(bool multi_threaded_message_loop)
    : multi_threaded_message_loop_(multi_threaded_message_loop),
      created_ui_thread_(false),
      created_io_thread_(false),
      created_file_thread_(false) {
}

CefProcess::~CefProcess() {
  // Terminate the IO thread.
  io_thread_.reset();

  // Terminate the FILE thread.
  file_thread_.reset();

  if (!multi_threaded_message_loop_) {
    // Must explicitly clean up the UI thread.
    ui_thread_->CleanUp();

    // Terminate the UI thread.
    ui_thread_.reset();

    // Terminate the message loop.
    ui_message_loop_.reset();
  }
}

void CefProcess::DoMessageLoopIteration() {
  DCHECK(CalledOnValidThread() && ui_message_loop_.get() != NULL);
  DCHECK(ui_message_loop_->is_iterating());
  ui_message_loop_->DoMessageLoopIteration();
}

void CefProcess::RunMessageLoop() {
  DCHECK(CalledOnValidThread() && ui_message_loop_.get() != NULL);
  ui_message_loop_->RunMessageLoop();
}

void CefProcess::QuitMessageLoop() {
  DCHECK(CalledOnValidThread() && ui_message_loop_.get() != NULL);
  ui_message_loop_->Quit();
}

void CefProcess::CreateUIThread() {
  DCHECK(!created_ui_thread_ && ui_thread_.get() == NULL);
  created_ui_thread_ = true;

  scoped_ptr<CefProcessUIThread> thread;
  if (multi_threaded_message_loop_) {
    // Create the message loop on a new thread.
    thread.reset(new CefProcessUIThread());
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_UI;
    if (!thread->StartWithOptions(options))
      return;
  } else {
    // Create the message loop on the current (main application) thread.
    ui_message_loop_.reset(new CefMessageLoopForUI());
    thread.reset(
        new CefProcessUIThread(ui_message_loop_.get()));

    // Must explicitly initialize the UI thread.
    thread->Init();
  }

  ui_thread_.swap(thread);
}

void CefProcess::CreateIOThread() {
  DCHECK(!created_io_thread_ && io_thread_.get() == NULL);
  created_io_thread_ = true;

  scoped_ptr<CefProcessIOThread> thread(new CefProcessIOThread());
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  io_thread_.swap(thread);
}

void CefProcess::CreateFileThread() {
  DCHECK(!created_file_thread_ && file_thread_.get() == NULL);
  created_file_thread_ = true;

  scoped_ptr<base::Thread> thread(new CefProcessSubThread(CefThread::FILE));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  file_thread_.swap(thread);
}
