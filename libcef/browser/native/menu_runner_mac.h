// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_MAC_H_
#define CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_MAC_H_
#pragma once

#include "libcef/browser/menu_runner.h"

#if __OBJC__
@class MenuControllerCocoa;
#else
class MenuControllerCocoa;
#endif

class CefMenuRunnerMac : public CefMenuRunner {
 public:
  CefMenuRunnerMac();
  ~CefMenuRunnerMac() override;

  // CefMenuRunner methods.
  bool RunContextMenu(AlloyBrowserHostImpl* browser,
                      CefMenuModelImpl* model,
                      const content::ContextMenuParams& params) override;
  void CancelContextMenu() override;

 private:
  MenuControllerCocoa* __strong menu_controller_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_MENU_RUNNER_MAC_H_
