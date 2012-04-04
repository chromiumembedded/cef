// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/cef_thread.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

#if defined(OS_WIN)
#include <Objbase.h>  // NOLINT(build/include_order)
#endif

using base::MessageLoopProxy;

// Friendly names for the well-known threads.
static const char* cef_thread_names[CefThread::ID_COUNT] = {
  "Cef_UIThread",  // UI
  "Cef_FileThread",  // FILE
  "Cef_IOThread",  // IO
};

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
                               int64 delay_ms) OVERRIDE {
    return CefThread::PostDelayedTask(id_, from_here, task, delay_ms);
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    return CefThread::PostDelayedTask(id_, from_here, task, delay);
  }

  // SequencedTaskRunner implementation.
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms) OVERRIDE {
    return CefThread::PostNonNestableDelayedTask(id_, from_here, task,
                                                 delay_ms);
  }

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


base::Lock CefThread::lock_;

CefThread* CefThread::cef_threads_[ID_COUNT];

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
  base::AutoLock lock(lock_);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(cef_threads_[identifier_] == NULL);
  cef_threads_[identifier_] = this;
}

CefThread::~CefThread() {
  // Stop the thread here, instead of the parent's class destructor.  This is so
  // that if there are pending tasks that run, code that checks that it's on the
  // correct CefThread succeeds.
  Stop();

  base::AutoLock lock(lock_);
  cef_threads_[identifier_] = NULL;
#ifndef NDEBUG
  // Double check that the threads are ordererd correctly in the enumeration.
  for (int i = identifier_ + 1; i < ID_COUNT; ++i) {
    DCHECK(!cef_threads_[i]) <<
        "Threads must be listed in the reverse order that they die";
  }
#endif
}

// static
bool CefThread::IsWellKnownThread(ID identifier) {
  base::AutoLock lock(lock_);
  return (identifier >= 0 && identifier < ID_COUNT &&
          cef_threads_[identifier]);
}

// static
bool CefThread::CurrentlyOn(ID identifier) {
  base::AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);
  return cef_threads_[identifier] &&
         cef_threads_[identifier]->message_loop() == MessageLoop::current();
}

// static
bool CefThread::PostTask(ID identifier,
                         const tracked_objects::Location& from_here,
                         const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, 0, true);
}

// static
bool CefThread::PostDelayedTask(ID identifier,
                                const tracked_objects::Location& from_here,
                                const base::Closure& task,
                                int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, true);
}

// static
bool CefThread::PostDelayedTask(ID identifier,
                                const tracked_objects::Location& from_here,
                                const base::Closure& task,
                                base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, task, delay.InMilliseconds(),
                        true);
}

// static
bool CefThread::PostNonNestableTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return PostTaskHelper(identifier, from_here, task, 0, false);
}

// static
bool CefThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int64 delay_ms) {
  return PostTaskHelper(identifier, from_here, task, delay_ms, false);
}

// static
bool CefThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return PostTaskHelper(identifier, from_here, task, delay.InMilliseconds(),
                        false);
}

// static
bool CefThread::GetCurrentThreadIdentifier(ID* identifier) {
  MessageLoop* cur_message_loop = MessageLoop::current();
  for (int i = 0; i < ID_COUNT; ++i) {
    if (cef_threads_[i] &&
        cef_threads_[i]->message_loop() == cur_message_loop) {
      *identifier = cef_threads_[i]->identifier_;
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
    int64 delay_ms,
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

  if (!guaranteed_to_outlive_target_thread)
    lock_.Acquire();

  MessageLoop* message_loop = cef_threads_[identifier] ?
      cef_threads_[identifier]->message_loop() : NULL;
  if (message_loop) {
    if (nestable) {
      message_loop->PostDelayedTask(from_here, task, delay_ms);
    } else {
      message_loop->PostNonNestableDelayedTask(from_here, task, delay_ms);
    }
  }

  if (!guaranteed_to_outlive_target_thread)
    lock_.Release();

  return !!message_loop;
}
