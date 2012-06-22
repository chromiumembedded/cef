// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_CEF_PROCESS_UI_THREAD_H_
#define CEF_LIBCEF_CEF_PROCESS_UI_THREAD_H_
#pragma once

#include "libcef/cef_thread.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "net/base/network_change_notifier.h"

class BrowserWebKitInit;
namespace base {
class StatsTable;
}

// ----------------------------------------------------------------------------
// CefProcessUIThread
//
// This simple thread object is used for the specialized threads that the
// CefProcess spins up.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class CefProcessUIThread
    : public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public CefThread {
 public:
  CefProcessUIThread();
  explicit CefProcessUIThread(MessageLoop* message_loop);
  virtual ~CefProcessUIThread();

  virtual void Init();
  virtual void CleanUp();

 private:
  void PlatformInit();
  void PlatformCleanUp();

  // From net::NetworkChangeNotifier::ConnectionTypeObserver
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type);

  base::StatsTable* statstable_;

  // WebKit implementation class.
  BrowserWebKitInit* webkit_init_;

  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(CefProcessUIThread);
};

#endif  // CEF_LIBCEF_CEF_PROCESS_UI_THREAD_H_
