// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class Thread;
}

namespace ui {
class Clipboard;
}

namespace content {
struct MainFunctionParams;
}

class CefBrowserContext;
class CefDevToolsDelegate;

class CefBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit CefBrowserMainParts(const content::MainFunctionParams& parameters);
  virtual ~CefBrowserMainParts();

  virtual void PreEarlyInitialization() OVERRIDE {}
  virtual void PostEarlyInitialization() OVERRIDE {}
  virtual void PreMainMessageLoopStart() OVERRIDE {}
  virtual MessageLoop* GetMainMessageLoop() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE {}
  virtual void ToolkitInitialized() OVERRIDE {}
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE {}

  ui::Clipboard* GetClipboard();
  CefBrowserContext* browser_context() const { return browser_context_.get(); }

 private:
  void PlatformInitialize();
  void PlatformCleanup();

  scoped_ptr<CefBrowserContext> browser_context_;

  scoped_ptr<ui::Clipboard> clipboard_;
  CefDevToolsDelegate* devtools_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMainParts);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_MAIN_H_
