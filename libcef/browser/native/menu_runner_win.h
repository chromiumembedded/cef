// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_WIN_H_
#define CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_WIN_H_
#pragma once

#include "libcef/browser/menu_runner.h"

#include "libcef/browser/native/native_menu_win.h"


class CefMenuRunnerWin : public CefMenuRunner {
 public:
  CefMenuRunnerWin();

  // CefMenuRunner methods.
  bool RunContextMenu(CefBrowserHostImpl* browser,
                      CefMenuModelImpl* model,
                      const content::ContextMenuParams& params) override;

 private:
  std::unique_ptr<views::CefNativeMenuWin> menu_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_WIN_H_
