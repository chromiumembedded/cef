// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_LINUX_H_
#define CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_LINUX_H_
#pragma once

#include "libcef/browser/menu_runner.h"

#include "ui/views/controls/menu/menu_runner.h"

class CefMenuRunnerLinux: public CefMenuRunner {
 public:
  CefMenuRunnerLinux();

  // CefMenuRunner methods.
  bool RunContextMenu(CefBrowserHostImpl* browser,
                      CefMenuModelImpl* model,
                      const content::ContextMenuParams& params) override;
  void CancelContextMenu() override;
  bool FormatLabel(base::string16& label) override;

 private:
  std::unique_ptr<views::MenuRunner> menu_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_LINUX_H_
