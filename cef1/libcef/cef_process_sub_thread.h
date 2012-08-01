// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_CEF_PROCESS_SUB_THREAD_H_
#define CEF_LIBCEF_CEF_PROCESS_SUB_THREAD_H_
#pragma once

#include "libcef/cef_thread.h"

#include "base/basictypes.h"

// ----------------------------------------------------------------------------
// CefProcessSubThread
//
// This simple thread object is used for the specialized threads that the
// CefProcess spins up.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class CefProcessSubThread : public CefThread {
 public:
  explicit CefProcessSubThread(CefThread::ID identifier);
  CefProcessSubThread(CefThread::ID identifier, MessageLoop* message_loop);
  virtual ~CefProcessSubThread();

 protected:
  virtual void CleanUp();

 private:
  DISALLOW_COPY_AND_ASSIGN(CefProcessSubThread);
};

#endif  // CEF_LIBCEF_CEF_PROCESS_SUB_THREAD_H_
