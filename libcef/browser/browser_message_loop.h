// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_LOOP_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_LOOP_H_
#pragma once

#include "base/macros.h"
#include "base/message_loop/message_loop.h"

// Class used to process events on the current message loop.
class CefBrowserMessageLoop : public base::MessageLoopForUI {
  typedef base::MessageLoopForUI inherited;

 public:
  CefBrowserMessageLoop();
  ~CefBrowserMessageLoop() override;

  // Returns the CefBrowserMessageLoop of the current thread.
  static CefBrowserMessageLoop* current();

  // Do a single interation of the UI message loop.
  void DoMessageLoopIteration();

  // Run the UI message loop.
  void RunMessageLoop();

 private:
  DISALLOW_COPY_AND_ASSIGN(CefBrowserMessageLoop);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_LOOP_H_
