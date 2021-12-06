// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_RUNNER_H_
#define CEF_LIBCEF_BROWSER_MENU_RUNNER_H_

#include <memory>
#include <string>

namespace content {
struct ContextMenuParams;
}

class AlloyBrowserHostImpl;
class CefMenuModelImpl;

// Provides platform-specific menu implementations for CefMenuCreator.
class CefMenuRunner {
 public:
  CefMenuRunner(const CefMenuRunner&) = delete;
  CefMenuRunner& operator=(const CefMenuRunner&) = delete;

  virtual bool RunContextMenu(AlloyBrowserHostImpl* browser,
                              CefMenuModelImpl* model,
                              const content::ContextMenuParams& params) = 0;
  virtual void CancelContextMenu() {}
  virtual bool FormatLabel(std::u16string& label) { return false; }

 protected:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<CefMenuRunner>;

  CefMenuRunner() = default;
  virtual ~CefMenuRunner() = default;
};

#endif  // CEF_LIBCEF_BROWSER_MENU_RUNNER_H_
