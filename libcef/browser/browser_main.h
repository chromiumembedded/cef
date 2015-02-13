// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "libcef/browser/browser_pref_store.h"
#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/url_request_context_getter_impl.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "content/public/browser/browser_main_parts.h"
#include "net/proxy/proxy_config_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoop;
class Thread;
}

namespace content {
struct MainFunctionParams;
}

class CefDevToolsDelegate;

class CefBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit CefBrowserMainParts(const content::MainFunctionParams& parameters);
  ~CefBrowserMainParts() override;

  void PreMainMessageLoopStart() override;
  void PostMainMessageLoopStart() override;
  void PreEarlyInitialization() override;
  void ToolkitInitialized() override;
  int PreCreateThreads() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  scoped_refptr<CefBrowserContextImpl> browser_context() const {
    return global_browser_context_;
  }
  scoped_refptr<CefURLRequestContextGetterImpl> request_context() const {
    return global_request_context_;
  }
  CefDevToolsDelegate* devtools_delegate() const {
    return devtools_delegate_;
  }
  PrefService* pref_service() const { return pref_service_.get(); }
  scoped_ptr<net::ProxyConfigService> proxy_config_service() {
    return proxy_config_service_.Pass();
  }

 private:
  void PlatformInitialize();
  void PlatformCleanup();

  scoped_refptr<CefBrowserContextImpl> global_browser_context_;
  scoped_refptr<CefURLRequestContextGetterImpl> global_request_context_;
  CefDevToolsDelegate* devtools_delegate_;  // Deletes itself.
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_refptr<CefBrowserPrefStore> pref_store_;
  scoped_ptr<PrefService> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMainParts);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
