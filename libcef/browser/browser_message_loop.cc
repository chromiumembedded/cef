// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_message_loop.h"
#include "base/run_loop.h"

CefBrowserMessageLoop::CefBrowserMessageLoop() {
}

CefBrowserMessageLoop::~CefBrowserMessageLoop() {
#if defined(OS_MACOSX)
  // On Mac the MessageLoop::AutoRunState scope in Run() never exits so clear
  // the run_loop_ variable to avoid an assertion in the MessageLoop destructor.
  run_loop_ = NULL;
#endif
}

// static
CefBrowserMessageLoop* CefBrowserMessageLoop::current() {
  MessageLoop* loop = MessageLoop::current();
  DCHECK_EQ(MessageLoop::TYPE_UI, loop->type());
  return static_cast<CefBrowserMessageLoop*>(loop);
}

void CefBrowserMessageLoop::DoMessageLoopIteration() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

void CefBrowserMessageLoop::RunMessageLoop() {
  Run();
}
