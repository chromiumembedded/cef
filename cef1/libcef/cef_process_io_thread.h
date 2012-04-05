// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_CEF_PROCESS_IO_THREAD_H_
#define CEF_LIBCEF_CEF_PROCESS_IO_THREAD_H_
#pragma once

#include "libcef/browser_request_context.h"
#include "libcef/cef_thread.h"

#include "base/basictypes.h"

namespace net {
class NetworkDelegate;
}

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
  CefProcessIOThread();
  explicit CefProcessIOThread(MessageLoop* message_loop);
  virtual ~CefProcessIOThread();

  scoped_refptr<BrowserRequestContext> request_context() {
    return request_context_;
  }

 protected:
  virtual void Init();
  virtual void CleanUp();

  scoped_refptr<BrowserRequestContext> request_context_;
  scoped_ptr<net::NetworkDelegate> network_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CefProcessIOThread);
};

#endif  // CEF_LIBCEF_CEF_PROCESS_IO_THREAD_H_
