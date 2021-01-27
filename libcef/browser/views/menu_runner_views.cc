// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/menu_runner_views.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/views/browser_view_impl.h"

CefMenuRunnerViews::CefMenuRunnerViews(CefBrowserViewImpl* browser_view)
    : browser_view_(browser_view) {}

bool CefMenuRunnerViews::RunContextMenu(
    AlloyBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  CefRefPtr<CefWindow> window = browser_view_->GetWindow();
  if (!window)
    return false;

  CefPoint screen_point(params.x, params.y);
  browser_view_->ConvertPointToScreen(screen_point);

  window->ShowMenu(model, screen_point, CEF_MENU_ANCHOR_TOPRIGHT);
  return true;
}

void CefMenuRunnerViews::CancelContextMenu() {
  CefRefPtr<CefWindow> window = browser_view_->GetWindow();
  if (window)
    window->CancelMenu();
}

bool CefMenuRunnerViews::FormatLabel(base::string16& label) {
  // Remove the accelerator indicator (&) from label strings.
  const base::string16::value_type replace[] = {L'&', 0};
  return base::ReplaceChars(label, replace, base::string16(), &label);
}
