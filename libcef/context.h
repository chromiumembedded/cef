// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "../include/cef.h"
#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/gfx/native_widget_types.h"
#include "webkit/glue/webpreferences.h"
#include "browser_webkit_init.h"

class CefBrowserImpl;

class CefContext : public CefThreadSafeBase<CefBase>
{
public:
  typedef std::list<CefRefPtr<CefBrowserImpl> > BrowserList;

  CefContext();
  ~CefContext();

  bool Initialize(bool multi_threaded_message_loop,
                  const std::wstring& cache_path);
  void Shutdown();

  MessageLoopForUI* GetMessageLoopForUI() { return messageloopui_; }

  HMODULE GetInstanceHandle() { return hinstance_; }
  HANDLE GetUIThreadHandle() { return hthreadui_; }
  DWORD GetUIThreadId() { return idthreadui_; }
  WebPreferences* GetWebPreferences() { return webprefs_; }
  
  // Retrieve the path at which cache data will be stored on disk.  If empty,
  // cache data will be stored in-memory.
  std::wstring GetCachePath() { return cache_path_; }

  bool AddBrowser(CefRefPtr<CefBrowserImpl> browser);
  bool RemoveBrowser(CefRefPtr<CefBrowserImpl> browser);
  BrowserList* GetBrowserList() { return &browserlist_; }

  // Returns true if the calling thread is the same as the UI thread
  bool RunningOnUIThread() { return (GetCurrentThreadId() == idthreadui_); }

  ////////////////////////////////////////////////////////////
  // ALL UIT_* METHODS MUST ONLY BE CALLED ON THE UI THREAD //
  ////////////////////////////////////////////////////////////

  void UIT_RegisterPlugin(struct CefPluginInfo* plugin_info);
  void UIT_UnregisterPlugin(struct CefPluginInfo* plugin_info);

  bool DoWork();
  bool DoDelayedWork();
  bool DoIdleWork();

private:
  void SetMessageLoopForUI(MessageLoopForUI* loop);
  void NotifyEvent();
  
  bool DoInitialize();
  void DoUninitialize();

protected:
  HMODULE hinstance_;
  DWORD idthreadui_;
  HANDLE hthreadui_;
  HANDLE heventui_;
  MessageLoopForUI* messageloopui_;
  bool in_transition_;
  BrowserList browserlist_;
  WebPreferences* webprefs_;
  StatsTable* statstable_;
  std::wstring cache_path_;

  // Initialize the AtExitManager to avoid asserts and possible memory leaks.
  base::AtExitManager at_exit_manager_;
  // Initialize WebKit for this scope.
  BrowserWebKitInit webkit_init_;
  
  friend DWORD WINAPI ThreadHandlerUI(LPVOID lpParam);
};

// Post a task to the UI message loop
void PostTask(const tracked_objects::Location& from_here, Task* task);

// Global context object pointer
extern CefRefPtr<CefContext> _Context;

// Macro for requiring that a function be called on the UI thread
#define REQUIRE_UIT()   DCHECK(_Context->RunningOnUIThread())

#endif // _CONTEXT_H
