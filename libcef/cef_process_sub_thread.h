// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CEF_PROCESS_SUB_THREAD
#define _CEF_PROCESS_SUB_THREAD

#include "base/basictypes.h"
#include "cef_thread.h"

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

#endif // _CEF_PROCESS_SUB_THREAD
