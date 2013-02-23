// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_message_loop.h"
#include "base/run_loop.h"

CefBrowserMessageLoop::CefBrowserMessageLoop() {
}

CefBrowserMessageLoop::~CefBrowserMessageLoop() {
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
