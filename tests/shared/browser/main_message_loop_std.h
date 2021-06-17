// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_MAIN_MESSAGE_LOOP_STD_H_
#define CEF_TESTS_SHARED_BROWSER_MAIN_MESSAGE_LOOP_STD_H_
#pragma once

#include "tests/shared/browser/main_message_loop.h"

namespace client {

// Represents the main message loop in the browser process. This implementation
// is a light-weight wrapper around the Chromium UI thread.
class MainMessageLoopStd : public MainMessageLoop {
 public:
  MainMessageLoopStd();

  // MainMessageLoop methods.
  int Run() override;
  void Quit() override;
  void PostTask(CefRefPtr<CefTask> task) override;
  bool RunsTasksOnCurrentThread() const override;

#if defined(OS_WIN)
  void SetCurrentModelessDialog(HWND hWndDialog) override;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(MainMessageLoopStd);
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_MAIN_MESSAGE_LOOP_STD_H_
