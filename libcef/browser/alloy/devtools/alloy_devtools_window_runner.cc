// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/devtools/alloy_devtools_window_runner.h"

#include "libcef/browser/alloy/devtools/devtools_frontend.h"
#include "libcef/browser/thread_util.h"

void AlloyDevToolsWindowRunner::ShowDevTools(
    CefBrowserHostBase* opener,
    std::unique_ptr<CefShowDevToolsParams> params) {
  CEF_REQUIRE_UIT();
  if (devtools_frontend_) {
    if (!params->inspect_element_at_.IsEmpty()) {
      devtools_frontend_->InspectElementAt(params->inspect_element_at_.x,
                                           params->inspect_element_at_.y);
    }
    devtools_frontend_->Focus();
    return;
  }

  auto alloy_browser = AlloyBrowserHostImpl::FromBaseChecked(opener);
  devtools_frontend_ = CefDevToolsFrontend::Show(
      alloy_browser.get(), params->window_info_, params->client_,
      params->settings_, params->inspect_element_at_,
      base::BindOnce(&AlloyDevToolsWindowRunner::OnFrontEndDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AlloyDevToolsWindowRunner::CloseDevTools() {
  CEF_REQUIRE_UIT();
  if (devtools_frontend_) {
    devtools_frontend_->Close();
  }
}

bool AlloyDevToolsWindowRunner::HasDevTools() {
  CEF_REQUIRE_UIT();
  return !!devtools_frontend_;
}

void AlloyDevToolsWindowRunner::OnFrontEndDestroyed() {
  CEF_REQUIRE_UIT();
  devtools_frontend_ = nullptr;
}
