// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window.h"

#include "include/base/cef_callback_helpers.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/shared/common/client_switches.h"

namespace client {

RootWindowConfig::RootWindowConfig(CefRefPtr<CefCommandLine> cmd)
    : command_line(cmd ? cmd : MainContext::Get()->GetCommandLine()),
      use_views(MainContext::Get()->UseViewsGlobal()),
      use_alloy_style(MainContext::Get()->UseAlloyStyleGlobal()),
      with_controls(!command_line->HasSwitch(switches::kHideControls)),
      url(MainContext::Get()->GetMainURL(command_line)) {}

RootWindow::RootWindow(bool use_alloy_style)
    : use_alloy_style_(use_alloy_style) {}

RootWindow::~RootWindow() = default;

// static
scoped_refptr<RootWindow> RootWindow::GetForBrowser(int browser_id) {
  return MainContext::Get()->GetRootWindowManager()->GetWindowForBrowser(
      browser_id);
}

bool RootWindow::IsWindowCreated() const {
  REQUIRE_MAIN_THREAD();
  return window_created_;
}

void RootWindow::SetPopupId(int opener_browser_id, int popup_id) {
  DCHECK_GT(opener_browser_id, 0);
  DCHECK_GT(popup_id, 0);
  opener_browser_id_ = opener_browser_id;
  popup_id_ = popup_id;
}

bool RootWindow::IsPopupIdMatch(int opener_browser_id, int popup_id) const {
  if (opener_browser_id_ == 0 || popup_id_ == 0) {
    // Not a popup.
    return false;
  }
  if (popup_id < 0) {
    // Only checking the opener.
    return opener_browser_id == opener_browser_id_;
  }
  return opener_browser_id == opener_browser_id_ && popup_id == popup_id_;
}

}  // namespace client
