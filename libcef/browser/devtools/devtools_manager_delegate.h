// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_

#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class BrowserContext;
}

class CefDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  static void StartHttpHandler(content::BrowserContext* browser_context);
  static void StopHttpHandler();

  CefDevToolsManagerDelegate();

  CefDevToolsManagerDelegate(const CefDevToolsManagerDelegate&) = delete;
  CefDevToolsManagerDelegate& operator=(const CefDevToolsManagerDelegate&) =
      delete;

  ~CefDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      content::DevToolsManagerDelegate::TargetType target_type) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_DELEGATE_H_
