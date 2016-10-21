// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class BrowserContext;
}

class CefDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  static void StartHttpHandler(content::BrowserContext* browser_context);
  static void StopHttpHandler();

  CefDevToolsManagerDelegate();
  ~CefDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(const GURL& url)
      override;
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefDevToolsManagerDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
