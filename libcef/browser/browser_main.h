// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "libcef/browser/browser_pref_store.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "content/public/browser/browser_main_parts.h"
#include "net/proxy/proxy_config_service.h"

namespace base {
class Thread;
}

namespace content {
struct MainFunctionParams;
}

class CefBrowserContext;
class CefDevToolsDelegate;
class MessageLoop;

class CefBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit CefBrowserMainParts(const content::MainFunctionParams& parameters);
  virtual ~CefBrowserMainParts();

  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE;

  CefBrowserContext* browser_context() const { return browser_context_.get(); }
  CefDevToolsDelegate* devtools_delegate() const { return devtools_delegate_; }
  scoped_ptr<net::ProxyConfigService> proxy_config_service() {
    return proxy_config_service_.Pass();
  }

 private:
  void PlatformInitialize();
  void PlatformCleanup();

  scoped_ptr<CefBrowserContext> browser_context_;
  CefDevToolsDelegate* devtools_delegate_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<PrefProxyConfigTracker> pref_proxy_config_tracker_;
  scoped_ptr<net::ProxyConfigService> proxy_config_service_;
  scoped_refptr<BrowserPrefStore> user_prefs_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMainParts);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
