// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_MAIN_MESSAGE_LOOP_MULTITHREADED_GTK_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_MAIN_MESSAGE_LOOP_MULTITHREADED_GTK_H_
#pragma once

#include <queue>

#include <gdk/gdk.h>

#include "include/base/cef_lock.h"
#include "include/base/cef_platform_thread.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

// Represents the main message loop in the browser process when using multi-
// threaded message loop mode on Linux. In this mode there is no Chromium
// message loop running on the main application thread. Instead, this
// implementation utilizes a Glib context for running tasks.
class MainMessageLoopMultithreadedGtk : public MainMessageLoop {
 public:
  MainMessageLoopMultithreadedGtk();
  ~MainMessageLoopMultithreadedGtk();

  // MainMessageLoop methods.
  int Run() override;
  void Quit() override;
  void PostTask(CefRefPtr<CefTask> task) override;
  bool RunsTasksOnCurrentThread() const override;

 private:
  static int TriggerRunTasks(void* self);
  void RunTasks();
  void DoQuit();

  base::PlatformThreadId thread_id_;

  GMainContext* main_context_;
  GMainLoop* main_loop_;

  base::Lock lock_;

  // Must be protected by |lock_|.
  std::queue<CefRefPtr<CefTask>> queued_tasks_;

  DISALLOW_COPY_AND_ASSIGN(MainMessageLoopMultithreadedGtk);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_MAIN_MESSAGE_LOOP_MULTITHREADED_GTK_H_
