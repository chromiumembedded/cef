// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/native/menu_runner_linux.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "ui/gfx/geometry/point.h"

CefMenuRunnerLinux::CefMenuRunnerLinux() {}

bool CefMenuRunnerLinux::RunContextMenu(
    AlloyBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  menu_.reset(
      new views::MenuRunner(model->model(), views::MenuRunner::CONTEXT_MENU));

  const gfx::Point& screen_point =
      browser->GetScreenPoint(gfx::Point(params.x, params.y));

  views::Widget* parent_widget = nullptr;
  if (!browser->IsWindowless())
    parent_widget = browser->GetWindowWidget();

  menu_->RunMenuAt(parent_widget, nullptr, gfx::Rect(screen_point, gfx::Size()),
                   views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE);

  return true;
}

void CefMenuRunnerLinux::CancelContextMenu() {
  if (menu_)
    menu_->Cancel();
}

bool CefMenuRunnerLinux::FormatLabel(std::u16string& label) {
  // Remove the accelerator indicator (&) from label strings.
  const std::u16string::value_type replace[] = {u'&', 0};
  return base::ReplaceChars(label, replace, std::u16string(), &label);
}
