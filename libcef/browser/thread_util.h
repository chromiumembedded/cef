// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_THREAD_UTIL_H_
#define CEF_LIBCEF_BROWSER_THREAD_UTIL_H_
#pragma once

#include "base/location.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#define CEF_UIT content::BrowserThread::UI
#define CEF_IOT content::BrowserThread::IO

#define CEF_CURRENTLY_ON(id) content::BrowserThread::CurrentlyOn(id)
#define CEF_CURRENTLY_ON_UIT() CEF_CURRENTLY_ON(CEF_UIT)
#define CEF_CURRENTLY_ON_IOT() CEF_CURRENTLY_ON(CEF_IOT)

#define CEF_REQUIRE(id) DCHECK(CEF_CURRENTLY_ON(id))
#define CEF_REQUIRE_UIT() CEF_REQUIRE(CEF_UIT)
#define CEF_REQUIRE_IOT() CEF_REQUIRE(CEF_IOT)

#define CEF_REQUIRE_RETURN(id, var)              \
  if (!CEF_CURRENTLY_ON(id)) {                   \
    DCHECK(false) << "called on invalid thread"; \
    return var;                                  \
  }
#define CEF_REQUIRE_UIT_RETURN(var) CEF_REQUIRE_RETURN(CEF_UIT, var)
#define CEF_REQUIRE_IOT_RETURN(var) CEF_REQUIRE_RETURN(CEF_IOT, var)

#define CEF_REQUIRE_RETURN_VOID(id)              \
  if (!CEF_CURRENTLY_ON(id)) {                   \
    DCHECK(false) << "called on invalid thread"; \
    return;                                      \
  }
#define CEF_REQUIRE_UIT_RETURN_VOID() CEF_REQUIRE_RETURN_VOID(CEF_UIT)
#define CEF_REQUIRE_IOT_RETURN_VOID() CEF_REQUIRE_RETURN_VOID(CEF_IOT)

template <int id, std::enable_if_t<id == CEF_UIT, bool> = true>
auto CEF_TASK_RUNNER() {
  return content::GetUIThreadTaskRunner({});
}
template <int id, std::enable_if_t<id == CEF_IOT, bool> = true>
auto CEF_TASK_RUNNER() {
  return content::GetIOThreadTaskRunner({});
}

#define CEF_POST_TASK(id, task) CEF_TASK_RUNNER<id>()->PostTask(FROM_HERE, task)
#define CEF_POST_DELAYED_TASK(id, task, delay_ms)         \
  CEF_TASK_RUNNER<id>()->PostDelayedTask(FROM_HERE, task, \
                                         base::Milliseconds(delay_ms))

// Post a blocking task with the specified |priority|. Tasks that have not
// started executing at shutdown will never run. However, any task that has
// already begun executing when shutdown is invoked will be allowed to continue
// and will block shutdown until completion.
// Tasks posted with this method are not guaranteed to run sequentially. Use
// base::CreateSequencedTaskRunner instead if sequence is important.
// Sequenced runners at various priorities that always execute all pending tasks
// before shutdown are available via CefTaskRunnerManager and exposed by the CEF
// API.
#define CEF_POST_BLOCKING_TASK(priority, task)                 \
  base::ThreadPool::PostTask(                                  \
      FROM_HERE,                                               \
      {priority, base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, \
       base::MayBlock()},                                      \
      task)

// Post a blocking task that affects UI or responsiveness of future user
// interactions. Do not use if an immediate response to a user interaction is
// expected.
#define CEF_POST_USER_VISIBLE_TASK(task) \
  CEF_POST_BLOCKING_TASK(base::TaskPriority::USER_VISIBLE, task)

// Post a blocking task where the user won't notice if it takes an arbitrarily
// long time to complete.
#define CEF_POST_BACKGROUND_TASK(task) \
  CEF_POST_BLOCKING_TASK(base::TaskPriority::BEST_EFFORT, task)

// Assert that blocking is allowed on the current thread.
#define CEF_REQUIRE_BLOCKING()                   \
  base::ScopedBlockingCall scoped_blocking_call( \
      FROM_HERE, base::BlockingType::WILL_BLOCK)

// Same as IMPLEMENT_REFCOUNTING() but using the specified Destructor.
#define IMPLEMENT_REFCOUNTING_EX(ClassName, Destructor) \
 public:                                                \
  void AddRef() const override {                        \
    ref_count_.AddRef();                                \
  }                                                     \
  bool Release() const override {                       \
    if (ref_count_.Release()) {                         \
      Destructor::Destruct(this);                       \
      return true;                                      \
    }                                                   \
    return false;                                       \
  }                                                     \
  bool HasOneRef() const override {                     \
    return ref_count_.HasOneRef();                      \
  }                                                     \
  bool HasAtLeastOneRef() const override {              \
    return ref_count_.HasAtLeastOneRef();               \
  }                                                     \
                                                        \
 private:                                               \
  CefRefCount ref_count_

#define IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(ClassName) \
  IMPLEMENT_REFCOUNTING_EX(ClassName, content::BrowserThread::DeleteOnUIThread)

#define IMPLEMENT_REFCOUNTING_DELETE_ON_IOT(ClassName) \
  IMPLEMENT_REFCOUNTING_EX(ClassName, content::BrowserThread::DeleteOnIOThread)

#endif  // CEF_LIBCEF_BROWSER_THREAD_UTIL_H_
