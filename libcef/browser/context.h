// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_CONTEXT_H_
#pragma once

#include <list>
#include <map>
#include <string>

#include "include/cef_app.h"

#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"

namespace base {
class WaitableEvent;
}

namespace component_updater {
class ComponentUpdateService;
}

namespace content {
class ContentMainRunner;
}

namespace printing {
class PrintJobManager;
}

class CefBrowserHostImpl;
class CefBrowserInfoManager;
class CefMainDelegate;
class CefTraceSubscriber;

class CefContext {
 public:
  typedef std::list<CefRefPtr<CefBrowserHostImpl> > BrowserList;

  CefContext();
  ~CefContext();

  // Returns the singleton CefContext instance.
  static CefContext* Get();

  // These methods will be called on the main application thread.
  bool Initialize(const CefMainArgs& args,
                  const CefSettings& settings,
                  CefRefPtr<CefApp> application,
                  void* windows_sandbox_info);
  void Shutdown();

  // Returns true if the current thread is the initialization thread.
  bool OnInitThread();

  // Returns true if the context is initialized.
  bool initialized() { return initialized_; }

  // Returns true if the context is shutting down.
  bool shutting_down() { return shutting_down_; }

  const CefSettings& settings() const { return settings_; }

  printing::PrintJobManager* print_job_manager() const {
    return print_job_manager_.get();
  }

  component_updater::ComponentUpdateService* component_updater();

  CefTraceSubscriber* GetTraceSubscriber();

  // Populate the request context settings based on CefSettings and command-
  // line flags.
  void PopulateRequestContextSettings(CefRequestContextSettings* settings);

 private:
  void OnContextInitialized();

  // Performs shutdown actions that need to occur on the UI thread before any
  // threads are destroyed.
  void FinishShutdownOnUIThread(base::WaitableEvent* uithread_shutdown_event);

  // Destroys the main runner and related objects.
  void FinalizeShutdown();

  // Track context state.
  bool initialized_;
  bool shutting_down_;

  // The thread on which the context was initialized.
  base::PlatformThreadId init_thread_id_;

  CefSettings settings_;

  scoped_ptr<CefMainDelegate> main_delegate_;
  scoped_ptr<content::ContentMainRunner> main_runner_;
  scoped_ptr<CefTraceSubscriber> trace_subscriber_;
  scoped_ptr<CefBrowserInfoManager> browser_info_manager_;

  // Only accessed on the UI Thread.
  scoped_ptr<printing::PrintJobManager> print_job_manager_;

  // Initially only for Widevine components.
  scoped_ptr<component_updater::ComponentUpdateService> component_updater_;
};

// Helper macro that returns true if the global context is in a valid state.
#define CONTEXT_STATE_VALID() \
    (CefContext::Get() && CefContext::Get()->initialized() && \
     !CefContext::Get()->shutting_down())

#endif  // CEF_LIBCEF_BROWSER_CONTEXT_H_
