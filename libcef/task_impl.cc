// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_task.h"
#include "libcef/cef_thread.h"
#include "base/bind.h"

namespace {

int GetThreadId(CefThreadId threadId) {
  switch (threadId) {
  case TID_UI: return CefThread::UI;
  case TID_IO: return CefThread::IO;
  case TID_FILE: return CefThread::FILE;
  };
  NOTREACHED();
  return -1;
}

}  // namespace

bool CefCurrentlyOn(CefThreadId threadId) {
  int id = GetThreadId(threadId);
  if (id < 0)
    return false;

  return CefThread::CurrentlyOn(static_cast<CefThread::ID>(id));
}

bool CefPostTask(CefThreadId threadId, CefRefPtr<CefTask> task) {
  int id = GetThreadId(threadId);
  if (id < 0)
    return false;

  return CefThread::PostTask(static_cast<CefThread::ID>(id), FROM_HERE,
      base::Bind(&CefTask::Execute, task, threadId));
}

bool CefPostDelayedTask(CefThreadId threadId, CefRefPtr<CefTask> task,
                        int64 delay_ms) {
  int id = GetThreadId(threadId);
  if (id < 0)
    return false;

  return CefThread::PostDelayedTask(static_cast<CefThread::ID>(id), FROM_HERE,
      base::Bind(&CefTask::Execute, task, threadId), delay_ms);
}
