// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/wrapper/cef_closure_task.h"

#include <memory>

#include "include/base/cef_callback.h"

namespace {

class CefOnceClosureTask : public CefTask {
 public:
  explicit CefOnceClosureTask(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  CefOnceClosureTask(const CefOnceClosureTask&) = delete;
  CefOnceClosureTask& operator=(const CefOnceClosureTask&) = delete;

  // CefTask method
  void Execute() override { std::move(closure_).Run(); }

 private:
  base::OnceClosure closure_;

  IMPLEMENT_REFCOUNTING(CefOnceClosureTask);
};

class CefRepeatingClosureTask : public CefTask {
 public:
  explicit CefRepeatingClosureTask(const base::RepeatingClosure& closure)
      : closure_(closure) {}

  CefRepeatingClosureTask(const CefRepeatingClosureTask&) = delete;
  CefRepeatingClosureTask& operator=(const CefRepeatingClosureTask&) = delete;

  // CefTask method
  void Execute() override {
    closure_.Run();
    closure_.Reset();
  }

 private:
  base::RepeatingClosure closure_;

  IMPLEMENT_REFCOUNTING(CefRepeatingClosureTask);
};

}  // namespace

CefRefPtr<CefTask> CefCreateClosureTask(base::OnceClosure closure) {
  return new CefOnceClosureTask(std::move(closure));
}

CefRefPtr<CefTask> CefCreateClosureTask(const base::RepeatingClosure& closure) {
  return new CefRepeatingClosureTask(closure);
}

bool CefPostTask(CefThreadId threadId, base::OnceClosure closure) {
  return CefPostTask(threadId, new CefOnceClosureTask(std::move(closure)));
}

bool CefPostTask(CefThreadId threadId, const base::RepeatingClosure& closure) {
  return CefPostTask(threadId, new CefRepeatingClosureTask(closure));
}

bool CefPostDelayedTask(CefThreadId threadId,
                        base::OnceClosure closure,
                        int64 delay_ms) {
  return CefPostDelayedTask(
      threadId, new CefOnceClosureTask(std::move(closure)), delay_ms);
}

bool CefPostDelayedTask(CefThreadId threadId,
                        const base::RepeatingClosure& closure,
                        int64 delay_ms) {
  return CefPostDelayedTask(threadId, new CefRepeatingClosureTask(closure),
                            delay_ms);
}
