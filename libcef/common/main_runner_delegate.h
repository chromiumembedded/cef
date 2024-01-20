// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_MAIN_RUNNER_DELEGATE_H_
#define CEF_LIBCEF_COMMON_MAIN_RUNNER_DELEGATE_H_
#pragma once

#include "include/cef_app.h"

namespace base {
class RunLoop;
}

namespace content {
class ContentMainDelegate;
}

class CefMainRunnerDelegate {
 public:
  virtual content::ContentMainDelegate* GetContentMainDelegate() = 0;

  virtual void BeforeMainThreadInitialize(const CefMainArgs& args) {}
  virtual void BeforeMainThreadRun(bool multi_threaded_message_loop) {}
  virtual void BeforeMainMessageLoopRun(base::RunLoop* run_loop) {}
  virtual bool HandleMainMessageLoopQuit() { return false; }
  virtual void BeforeUIThreadInitialize() {}
  virtual void AfterUIThreadInitialize() {}
  virtual void BeforeUIThreadShutdown() {}
  virtual void AfterUIThreadShutdown() {}
  virtual void BeforeMainThreadShutdown() {}
  virtual void AfterMainThreadShutdown() {}
  virtual void BeforeExecuteProcess(const CefMainArgs& args) {}
  virtual void AfterExecuteProcess() {}

  virtual ~CefMainRunnerDelegate() = default;
};

#endif  // CEF_LIBCEF_COMMON_MAIN_RUNNER_DELEGATE_H_
