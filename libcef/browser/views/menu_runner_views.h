// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_MENU_RUNNER_VIEWS_H_
#define CEF_LIBCEF_BROWSER_VIEWS_MENU_RUNNER_VIEWS_H_
#pragma once

#include "libcef/browser/menu_runner.h"

class CefBrowserViewImpl;

class CefMenuRunnerViews: public CefMenuRunner {
 public:
  // |browser_view| is guaranteed to outlive this object.
  explicit CefMenuRunnerViews(CefBrowserViewImpl* browser_view);

  // CefMenuRunner methods.
  bool RunContextMenu(CefBrowserHostImpl* browser,
                      CefMenuModelImpl* model,
                      const content::ContextMenuParams& params) override;
  void CancelContextMenu() override;
  bool FormatLabel(base::string16& label) override;

 private:
  CefBrowserViewImpl* browser_view_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_MENU_RUNNER_VIEWS_H_
