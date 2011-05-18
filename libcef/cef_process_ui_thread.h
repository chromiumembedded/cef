// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CEF_PROCESS_UI_THREAD
#define _CEF_PROCESS_UI_THREAD

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cef_thread.h"
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
    : public net::NetworkChangeNotifier::OnlineStateObserver,
      public CefThread
{
 public:
  explicit CefProcessUIThread();
  CefProcessUIThread(MessageLoop* message_loop);
  virtual ~CefProcessUIThread();

  virtual void Init();
  virtual void CleanUp();

 private:
  void PlatformInit();
  void PlatformCleanUp();

  // From net::NetworkChangeNotifier::OnlineStateObserver
  void OnOnlineStateChanged(bool online);

  base::StatsTable* statstable_;

  // WebKit implementation class.
  BrowserWebKitInit* webkit_init_;

  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(CefProcessUIThread);
};

#endif // _CEF_PROCESS_UI_THREAD
