// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "libcef/browser/net/url_request_context_getter_impl.h"

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_main_parts.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoop;
class Thread;
}

namespace content {
struct MainFunctionParams;
}

namespace extensions {
class ExtensionsBrowserClient;
class ExtensionsClient;
}

#if defined(USE_AURA)
namespace wm {
class WMState;
}
#endif

class CefBrowserContextImpl;
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

  CefBrowserContextImpl* browser_context() const {
    return global_browser_context_.get();
  }
  CefDevToolsDelegate* devtools_delegate() const {
    return devtools_delegate_;
  }

 private:
#if defined(OS_WIN)
  void PlatformInitialize();
#endif  // defined(OS_WIN)

  std::unique_ptr<CefBrowserContextImpl> global_browser_context_;
  CefDevToolsDelegate* devtools_delegate_;  // Deletes itself.
  std::unique_ptr<base::MessageLoop> message_loop_;

  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::ExtensionsBrowserClient> extensions_browser_client_;

#if defined(USE_AURA)
  std::unique_ptr<wm::WMState> wm_state_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMainParts);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
