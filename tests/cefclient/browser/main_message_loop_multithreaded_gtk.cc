// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/main_message_loop_multithreaded_gtk.h"

#include <X11/Xlib.h>
#include <gtk/gtk.h>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/wrapper/cef_closure_task.h"

namespace client {

namespace {

base::Lock g_global_lock;
base::PlatformThreadId g_global_lock_thread = kInvalidPlatformThreadId;

void lock_enter() {
  // The GDK lock is not reentrant, so check that we're using it correctly.
  // See comments on ScopedGdkThreadsEnter.
  base::PlatformThreadId current_thread = base::PlatformThread::CurrentId();
  CHECK(current_thread != g_global_lock_thread);

  g_global_lock.Acquire();
  g_global_lock_thread = current_thread;
}

void lock_leave() {
  g_global_lock_thread = kInvalidPlatformThreadId;
  g_global_lock.Release();
}

// Same as g_idle_add() but specifying the GMainContext.
guint idle_add(GMainContext* main_context,
               GSourceFunc function,
               gpointer data) {
  GSource* source = g_idle_source_new();
  g_source_set_callback(source, function, data, nullptr);
  guint id = g_source_attach(source, main_context);
  g_source_unref(source);
  return id;
}

// Same as g_timeout_add() but specifying the GMainContext.
guint timeout_add(GMainContext* main_context,
                  guint interval,
                  GSourceFunc function,
                  gpointer data) {
  GSource* source = g_timeout_source_new(interval);
  g_source_set_callback(source, function, data, nullptr);
  guint id = g_source_attach(source, main_context);
  g_source_unref(source);
  return id;
}

}  // namespace

MainMessageLoopMultithreadedGtk::MainMessageLoopMultithreadedGtk()
    : thread_id_(base::PlatformThread::CurrentId()) {
  // Initialize Xlib support for concurrent threads. This function must be the
  // first Xlib function a multi-threaded program calls, and it must complete
  // before any other Xlib call is made.
  CHECK(XInitThreads() != 0);

  // Initialize GDK thread support. See comments on ScopedGdkThreadsEnter.
  gdk_threads_set_lock_functions(lock_enter, lock_leave);
  gdk_threads_init();
}

MainMessageLoopMultithreadedGtk::~MainMessageLoopMultithreadedGtk() {
  DCHECK(RunsTasksOnCurrentThread());
  DCHECK(queued_tasks_.empty());
}

int MainMessageLoopMultithreadedGtk::Run() {
  DCHECK(RunsTasksOnCurrentThread());

  // We use the default Glib context and Chromium creates its own context in
  // MessagePumpGlib (starting in M86).
  main_context_ = g_main_context_default();

  main_loop_ = g_main_loop_new(main_context_, TRUE);

  // Check the queue when GTK is idle, or at least every 100ms.
  // TODO(cef): It might be more efficient to use input functions
  // (gdk_input_add) and trigger by writing to an fd.
  idle_add(main_context_, MainMessageLoopMultithreadedGtk::TriggerRunTasks,
           this);
  timeout_add(main_context_, 100,
              MainMessageLoopMultithreadedGtk::TriggerRunTasks, this);

  // Block until g_main_loop_quit().
  g_main_loop_run(main_loop_);

  // Release GLib resources.
  g_main_loop_unref(main_loop_);
  main_loop_ = nullptr;

  main_context_ = nullptr;

  return 0;
}

void MainMessageLoopMultithreadedGtk::Quit() {
  PostTask(CefCreateClosureTask(base::BindOnce(
      &MainMessageLoopMultithreadedGtk::DoQuit, base::Unretained(this))));
}

void MainMessageLoopMultithreadedGtk::PostTask(CefRefPtr<CefTask> task) {
  base::AutoLock lock_scope(lock_);

  // Queue the task.
  queued_tasks_.push(task);
}

bool MainMessageLoopMultithreadedGtk::RunsTasksOnCurrentThread() const {
  return (thread_id_ == base::PlatformThread::CurrentId());
}

// static
int MainMessageLoopMultithreadedGtk::TriggerRunTasks(void* self) {
  static_cast<MainMessageLoopMultithreadedGtk*>(self)->RunTasks();
  return G_SOURCE_CONTINUE;
}

void MainMessageLoopMultithreadedGtk::RunTasks() {
  DCHECK(RunsTasksOnCurrentThread());

  std::queue<CefRefPtr<CefTask>> tasks;

  {
    base::AutoLock lock_scope(lock_);
    tasks.swap(queued_tasks_);
  }

  // Execute all queued tasks.
  while (!tasks.empty()) {
    CefRefPtr<CefTask> task = tasks.front();
    tasks.pop();
    task->Execute();
  }
}

void MainMessageLoopMultithreadedGtk::DoQuit() {
  DCHECK(RunsTasksOnCurrentThread());
  g_main_loop_quit(main_loop_);
}

}  // namespace client
