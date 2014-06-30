// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MANAGER_RUNNER_LINUX_H_
#define CEF_LIBCEF_BROWSER_MENU_MANAGER_RUNNER_LINUX_H_
#pragma once

#include "libcef/browser/menu_creator.h"

#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/menu/menu_runner.h"

class CefMenuCreatorRunnerLinux: public CefMenuCreator::Runner {
 public:
  CefMenuCreatorRunnerLinux();
  virtual ~CefMenuCreatorRunnerLinux();

  // CefMemoryManager::Runner methods.
  virtual bool RunContextMenu(CefMenuCreator* manager) OVERRIDE;
  virtual void CancelContextMenu() OVERRIDE;
  virtual bool FormatLabel(base::string16& label) OVERRIDE;

 private:
  scoped_ptr<views::MenuRunner> menu_;
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MANAGER_RUNNER_LINUX_H_
