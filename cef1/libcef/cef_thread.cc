// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/cef_thread.h"

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/threading/sequenced_worker_pool.h"

#if defined(OS_WIN)
#include <Objbase.h>  // NOLINT(build/include_order)
#endif

using base::MessageLoopProxy;

namespace {

// Friendly names for the well-known threads.
static const char* cef_thread_names[CefThread::ID_COUNT] = {
  "Cef_UIThread",  // UI
  "Cef_FileThread",  // FILE
  "Cef_IOThread",  // IO
};

struct CefThreadGlobals {
  CefThreadGlobals() {
    memset(threads, 0, CefThread::ID_COUNT * sizeof(threads[0]));
  }

  // This lock protects |threads|. Do not read or modify that array
  // without holding this lock. Do not block while holding this lock.
  base::Lock lock;

  // This array is protected by |lock|. The threads are not owned by this
  // array. Typically, the threads are owned on the UI thread by
  // BrowserMainLoop. BrowserThreadImpl objects remove themselves from this
  // array upon destruction.
  CefThread* threads[CefThread::ID_COUNT];

  scoped_refptr<base::SequencedWorkerPool> blocking_pool;
};

base::LazyInstance<CefThreadGlobals>::Leaky
    g_globals = LAZY_INSTANCE_INITIALIZER;

// An implementation of MessageLoopProxy to be used in conjunction
// with CefThread.
class CefThreadMessageLoopProxy : public MessageLoopProxy {
 public:
  explicit CefThreadMessageLoopProxy(CefThread::ID identifier)
      : id_(identifier) {
  }

  // TaskRunner implementation.
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    return CefThread::PostDelayedTask(id_, from_here, task, delay);
  }

  // SequencedTaskRunner implementation.
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    return CefThread::PostNonNestableDelayedTask(id_, from_here, task, delay);
  }

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return CefThread::CurrentlyOn(id_);
  }

 private:
  CefThread::ID id_;
  DISALLOW_COPY_AND_ASSIGN(CefThreadMessageLoopProxy);
};

}  // namespace

CefThread::CefThread(CefThread::ID identifier)
    : Thread(cef_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

CefThread::CefThread(ID identifier, MessageLoop* message_loop)
    : Thread(cef_thread_names[identifier]),
      identifier_(identifier) {
  message_loop->set_thread_name(cef_thread_names[identifier]);
  set_message_loop(message_loop);
  Initialize();
}

// static
void CefThread::CreateThreadPool() {
  CefThreadGlobals& globals = g_globals.Get();
  DCHECK(!globals.blocking_pool.get());
  globals.blocking_pool = new base::SequencedWorkerPool(3, "BrowserBlocking");
}

// static
void CefThread::ShutdownThreadPool() {
  // The goal is to make it impossible for chrome to 'infinite loop' during
  // shutdown, but to reasonably expect that all BLOCKING_SHUTDOWN tasks queued
  // during shutdown get run. There's nothing particularly scientific about the
  // number chosen.
  const int kMaxNewShutdownBlockingTasks = 1000;
  CefThreadGlobals& globals = g_globals.Get();
  globals.blocking_pool->Shutdown(kMaxNewShutdownBlockingTasks);
  globals.blocking_pool = NULL;
}

void CefThread::Init() {
#if defined(OS_WIN)
  // Initializes the COM library on the current thread.
  CoInitialize(NULL);
#endif

#if defined(OS_MACOSX)
  autorelease_pool_.reset(new base::mac::ScopedNSAutoreleasePool);
#endif
}

void CefThread::Cleanup() {
#if defined(OS_WIN)
  // Closes the COM library on the current thread. CoInitialize must
  // be balanced by a corresponding call to CoUninitialize.
  CoUninitialize();
#endif

#if defined(OS_MACOSX)
  autorelease_pool_.reset(NULL);
#endif
}

void CefThread::Initialize() {
  CefThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(globals.threads[identifier_] == NULL);
  globals.threads[identifier_] = this;
}

CefThread::~CefThread() {
  // Stop the thread here, instead of the parent's class destructor.  This is so
  // that if there are pending tasks that run, code that checks that it's on the
  // correct CefThread succeeds.
  Stop();

  CefThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  globals.threads[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordererd correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!globals.threads[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
base::SequencedWorkerPool* CefThread::GetBlockingPool() {
  CefThreadGlobals& globals = g_globals.Get();
  DCHECK(globals.blocking_pool.get());
  return globals.blocking_pool;
}

// static
bool CefThread::IsWellKnownThread(ID identifier) {
  CefThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  return (identifier >= 0 && identifier < ID_COUNT &&
          globals.threads[identifier]);
}

// static
bool CefThread::CurrentlyOn(ID identifier) {
  CefThreadGlobals& globals = g_globals.Get();
  base::AutoLock lock(globals.lock);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return globals.threads[identifier] &&
         globals.threads[identifier]->message_loop() ==
            base::MessageLoop::current();
}

// static
bool CefThread::PostTask(ID identifier,
                         const tracked_objects::Location& from_here,
                         const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, base::TimeDelta(), true);
}

// static
bool CefThread::PostDelayedTask(ID identifier,
                                const tracked_objects::Location& from_here,
                                const base::Closure& task,
                                base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, task, delay, true);
}

// static
bool CefThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, base::TimeDelta(), false);
}

// static
bool CefThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, task, delay, false);
}

// static
bool CefThread::GetCurrentThreadIdentifier(ID* identifier) {
  CefThreadGlobals& globals = g_globals.Get();
  base::MessageLoop* cur_message_loop = base::MessageLoop::current();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (globals.threads[i] &&
        globals.threads[i]->message_loop() == cur_message_loop) {
      *identifier = globals.threads[i]->identifier_;
      return true;
    }
  }

  return false;
}

// static
scoped_refptr<MessageLoopProxy> CefThread::GetMessageLoopProxyForThread(
    ID identifier) {
  scoped_refptr<MessageLoopProxy> proxy =
      new CefThreadMessageLoopProxy(identifier);
  return proxy;
}

// static
bool CefThread::PostTaskHelper(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay,
    bool nestable) {
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  // Optimization: to avoid unnecessary locks, we listed the ID enumeration in
  // order of lifetime.  So no need to lock if we know that the other thread
  // outlives this one.
  // Note: since the array is so small, ok to loop instead of creating a map,
  // which would require a lock because std::map isn't thread safe, defeating
  // the whole purpose of this optimization.
  ID current_thread;
  bool guaranteed_to_outlive_target_thread =
      GetCurrentThreadIdentifier(&current_thread) &&
      current_thread >= identifier;

  CefThreadGlobals& globals = g_globals.Get();

  if (!guaranteed_to_outlive_target_thread)
    globals.lock.Acquire();

  base::MessageLoop* message_loop = globals.threads[identifier] ?
      globals.threads[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    globals.lock.Release();

  return !!message_loop;
}
