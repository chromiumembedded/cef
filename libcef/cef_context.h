// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _CEF_CONTEXT_H
#define _CEF_CONTEXT_H

#include "../include/cef.h"
#include "browser_request_context.h"
#include "cef_process.h"
#include "cef_thread.h"

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include <map>

class BrowserRequestContext;
class CefBrowserImpl;
struct WebPreferences;

class CefContext : public CefThreadSafeBase<CefBase>
{
public:
  typedef std::list<CefRefPtr<CefBrowserImpl> > BrowserList;
  
  CefContext();
  ~CefContext();

  // These methods will be called on the main application thread.
  bool Initialize(bool multi_threaded_message_loop,
                  const std::wstring& cache_path);
  void Shutdown();

  scoped_refptr<CefProcess> process() { return process_; }

  bool AddBrowser(CefRefPtr<CefBrowserImpl> browser);
  bool RemoveBrowser(CefRefPtr<CefBrowserImpl> browser);
  CefRefPtr<CefBrowserImpl> GetBrowserByID(int id);
  BrowserList* GetBrowserList() { return &browserlist_; }

  // Retrieve the path at which cache data will be stored on disk.  If empty,
  // cache data will be stored in-memory.
  const std::wstring& cache_path() { return cache_path_; }

  WebPreferences* web_preferences()
  {
    REQUIRE_UIT();
    return webprefs_;
  }

  // The BrowserRequestContext object is managed by CefProcessIOThread.
  void set_request_context(BrowserRequestContext* request_context)
    { request_context_ = request_context; }
  scoped_refptr<BrowserRequestContext> request_context()
    { return request_context_; }

private:
  // Manages the various process threads.
  scoped_refptr<CefProcess> process_;

  // Initialize the AtExitManager on the main application thread to avoid
  // asserts and possible memory leaks.
  base::AtExitManager at_exit_manager_;

  std::wstring cache_path_;
  WebPreferences* webprefs_;
  scoped_refptr<BrowserRequestContext> request_context_;
  
  // Map of browsers that currently exist.
  BrowserList browserlist_;
  
  // Used for assigning unique IDs to browser instances.
  int next_browser_id_;
};

// Global context object pointer
extern CefRefPtr<CefContext> _Context;

#endif // _CEF_CONTEXT_H
