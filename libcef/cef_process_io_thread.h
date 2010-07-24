// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CEF_PROCESS_IO_THREAD
#define _CEF_PROCESS_IO_THREAD

#include "base/basictypes.h"
#include "cef_thread.h"
#include "browser_request_context.h"

// ----------------------------------------------------------------------------
// CefProcessIOThread
//
// This simple thread object is used for the specialized threads that the
// CefProcess spins up.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class CefProcessIOThread : public CefThread {
 public:
  explicit CefProcessIOThread();
  CefProcessIOThread(MessageLoop* message_loop);
  virtual ~CefProcessIOThread();

  virtual void Init();
  virtual void CleanUp();

  scoped_refptr<BrowserRequestContext> request_context()
    { return request_context_; }

 private:
  scoped_refptr<BrowserRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(CefProcessIOThread);
};

#endif // _CEF_PROCESS_UI_THREAD
