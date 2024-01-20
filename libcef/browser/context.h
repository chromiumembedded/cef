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
#include "libcef/browser/main_runner.h"

#include "base/observer_list.h"
#include "base/threading/platform_thread.h"
#include "third_party/skia/include/core/SkColor.h"

class CefBrowserInfoManager;
class CefTraceSubscriber;

class CefContext {
 public:
  // Interface to implement for observers that wish to be informed of changes
  // to the context. All methods will be called on the UI thread.
  class Observer {
   public:
    // Called before the context is destroyed.
    virtual void OnContextDestroyed() = 0;

   protected:
    virtual ~Observer() = default;
  };

  CefContext();
  ~CefContext();

  // Returns the singleton CefContext instance.
  static CefContext* Get();

  // These methods will be called on the main application thread.
  bool Initialize(const CefMainArgs& args,
                  const CefSettings& settings,
                  CefRefPtr<CefApp> application,
                  void* windows_sandbox_info);
  void RunMessageLoop();
  void QuitMessageLoop();
  void Shutdown();

  // Returns true if the current thread is the initialization thread.
  bool OnInitThread();

  // Returns true if the context is initialized.
  bool initialized() { return initialized_; }

  // Returns true if the context is shutting down.
  bool shutting_down() { return shutting_down_; }

  const CefSettings& settings() const { return settings_; }

  // Returns the background color for the browser. If |browser_settings| is
  // nullptr or does not specify a color then the global settings will be used.
  // The alpha component will be either SK_AlphaTRANSPARENT or SK_AlphaOPAQUE
  // (e.g. fully transparent or fully opaque). If |is_windowless| is
  // STATE_DISABLED then SK_AlphaTRANSPARENT will always be returned. If
  // |is_windowless| is STATE_ENABLED then SK_ColorTRANSPARENT may be returned
  // to enable transparency for windowless browsers. See additional comments on
  // CefSettings.background_color and CefBrowserSettings.background_color.
  SkColor GetBackgroundColor(const CefBrowserSettings* browser_settings,
                             cef_state_t windowless_state) const;

  CefTraceSubscriber* GetTraceSubscriber();

  // Populate request context settings for the global system context based on
  // CefSettings and command-line flags.
  void PopulateGlobalRequestContextSettings(
      CefRequestContextSettings* settings);

  // Normalize and validate request context settings for user-created contexts.
  void NormalizeRequestContextSettings(CefRequestContextSettings* settings);

  // Manage observer objects. The observer must either outlive this object or
  // remove itself before destruction. These methods can only be called on the
  // UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  void OnContextInitialized();

  // Performs shutdown actions that need to occur on the UI thread before any
  // threads are destroyed.
  void ShutdownOnUIThread();

  // Destroys the main runner and related objects.
  void FinalizeShutdown();

  // Track context state.
  bool initialized_ = false;
  bool shutting_down_ = false;

  // The thread on which the context was initialized.
  base::PlatformThreadId init_thread_id_ = 0;

  CefSettings settings_;
  CefRefPtr<CefApp> application_;

  std::unique_ptr<CefMainRunner> main_runner_;
  std::unique_ptr<CefTraceSubscriber> trace_subscriber_;
  std::unique_ptr<CefBrowserInfoManager> browser_info_manager_;

  // Observers that want to be notified of changes to this object.
  base::ObserverList<Observer>::Unchecked observers_;
};

// Helper macro that returns true if the global context is in a valid state.
#define CONTEXT_STATE_VALID()                               \
  (CefContext::Get() && CefContext::Get()->initialized() && \
   !CefContext::Get()->shutting_down())

#endif  // CEF_LIBCEF_BROWSER_CONTEXT_H_
