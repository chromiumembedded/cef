// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/devtools/devtools_window_runner.h"

#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/devtools/devtools_window.h"

void CefDevToolsWindowRunner::ShowDevTools(
    CefBrowserHostBase* opener,
    std::unique_ptr<CefShowDevToolsParams> params) {
  CEF_REQUIRE_UIT();
  auto* web_contents = opener->GetWebContents();
  if (!web_contents) {
    return;
  }

  auto* profile = CefRequestContextImpl::GetProfile(opener->request_context());
  if (!DevToolsWindow::AllowDevToolsFor(profile, web_contents)) {
    LOG(WARNING) << "DevTools is not allowed for this browser";
    return;
  }

  auto inspect_element_at = params->inspect_element_at_;

  if (!browser_host_) {
    // Configure parameters for ChromeBrowserDelegate::CreateDevToolsBrowser
    // which will be called indirectly to create the DevTools window.
    DCHECK(!pending_params_);
    pending_params_ = std::move(params);
  }

  // Focus the existing DevTools window or create a new one.
  if (!inspect_element_at.IsEmpty()) {
    DevToolsWindow::InspectElement(web_contents->GetPrimaryMainFrame(),
                                   inspect_element_at.x, inspect_element_at.y);
  } else {
    DevToolsWindow::OpenDevToolsWindow(web_contents, profile,
                                       DevToolsOpenedByAction::kUnknown);
  }

  // The DevTools browser host should now exist.
  DCHECK(browser_host_);
}

void CefDevToolsWindowRunner::CloseDevTools() {
  CEF_REQUIRE_UIT();
  if (browser_host_) {
    browser_host_->TryCloseBrowser();
    browser_host_ = nullptr;
  }
}

bool CefDevToolsWindowRunner::HasDevTools() {
  CEF_REQUIRE_UIT();
  return !!browser_host_;
}

std::unique_ptr<CefShowDevToolsParams>
CefDevToolsWindowRunner::TakePendingParams() {
  CEF_REQUIRE_UIT();
  return std::move(pending_params_);
}

void CefDevToolsWindowRunner::SetDevToolsBrowserHost(
    base::WeakPtr<ChromeBrowserHostImpl> browser_host) {
  CEF_REQUIRE_UIT();
  DCHECK(!browser_host_);
  browser_host_ = browser_host;
}
