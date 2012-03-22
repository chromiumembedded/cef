// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _CEF_CONTEXT_H
#define _CEF_CONTEXT_H

#include "include/cef.h"
#include "browser_file_system.h"
#include "browser_request_context.h"
#include "cef_process.h"
#include "dom_storage_context.h"

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include <map>

class CefBrowserImpl;
class WebViewHost;

namespace base {
class WaitableEvent;
}

class CefContext : public CefBase
{
public:
  typedef std::list<CefRefPtr<CefBrowserImpl> > BrowserList;
  
  CefContext();
  ~CefContext();

  // These methods will be called on the main application thread.
  bool Initialize(const CefSettings& settings, CefRefPtr<CefApp> application);
  void Shutdown();

  // Returns true if the context is initialized.
  bool initialized() { return initialized_; }

  // Returns true if the context is shutting down.
  bool shutting_down() { return shutting_down_; }

  CefProcess* process() { return process_.get(); }

  bool AddBrowser(CefRefPtr<CefBrowserImpl> browser);
  bool RemoveBrowser(CefRefPtr<CefBrowserImpl> browser);
  CefRefPtr<CefBrowserImpl> GetBrowserByID(int id);
  BrowserList* GetBrowserList() { return &browserlist_; }

  // Retrieve the path at which cache data will be stored on disk.  If empty,
  // cache data will be stored in-memory.
  const FilePath& cache_path() const { return cache_path_; }

  const CefSettings& settings() const { return settings_; }
  CefRefPtr<CefApp> application() const { return application_; }

  // Return the locale specified in CefSettings or the default value of "en-US".
  std::string locale() const;

  // The BrowserRequestContext object is managed by CefProcessIOThread.
  void set_request_context(BrowserRequestContext* request_context)
    { request_context_ = request_context; }
  BrowserRequestContext* request_context() { return request_context_; }

  // The DOMStorageContext object is managed by CefProcessUIThread.
  void set_storage_context(DOMStorageContext* storage_context)
    { storage_context_.reset(storage_context); }
  DOMStorageContext* storage_context() { return storage_context_.get(); }

  BrowserFileSystem* file_system() { return &file_system_; }

  // Used to keep track of the web view host we're dragging over. WARNING:
  // this pointer should never be dereferenced.  Use it only for comparing
  // pointers.
  WebViewHost* current_webviewhost() { return current_webviewhost_; }
  void set_current_webviewhost(WebViewHost* host)
      { current_webviewhost_ = host; }

  static bool ImplementsThreadSafeReferenceCounting() { return true; }

private:
  // Performs shutdown actions that need to occur on the UI thread before any
  // threads are destroyed.
  void UIT_FinishShutdown(base::WaitableEvent* browser_shutdown_event,
                          base::WaitableEvent* uithread_shutdown_event);

  // Track context state.
  bool initialized_;
  bool shutting_down_;

  // Manages the various process threads.
  scoped_ptr<CefProcess> process_;

  // Initialize the AtExitManager on the main application thread to avoid
  // asserts and possible memory leaks.
  base::AtExitManager at_exit_manager_;

  CefSettings settings_;
  CefRefPtr<CefApp> application_;
  FilePath cache_path_;
  scoped_refptr<BrowserRequestContext> request_context_;
  scoped_ptr<DOMStorageContext> storage_context_;
  BrowserFileSystem file_system_;

  // Map of browsers that currently exist.
  BrowserList browserlist_;

  // Used for assigning unique IDs to browser instances.
  int next_browser_id_;

  WebViewHost* current_webviewhost_;

  IMPLEMENT_REFCOUNTING(CefContext);
  IMPLEMENT_LOCKING(CefContext);
};

// Global context object pointer.
extern CefRefPtr<CefContext> _Context;

// Helper macro that returns true if the global context is in a valid state.
#define CONTEXT_STATE_VALID() \
    (_Context.get() && _Context->initialized() && !_Context->shutting_down())

#endif // _CEF_CONTEXT_H
