// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/native/menu_runner_views_aura.h"

#include <memory>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

#include "base/strings/string_util.h"
#include "ui/gfx/geometry/point.h"

CefMenuRunnerViewsAura::CefMenuRunnerViewsAura() = default;

bool CefMenuRunnerViewsAura::RunContextMenu(
    AlloyBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  views::Widget* widget = nullptr;
  gfx::AcceleratedWidget parent_widget = gfx::kNullAcceleratedWidget;
  if (browser->IsWindowless()) {
    parent_widget = browser->GetWindowHandle();
    if (!parent_widget) {
      LOG(ERROR) << "Window handle is required for default OSR context menu.";
      return false;
    }
  } else {
    widget = browser->GetWindowWidget();
  }

  menu_ = std::make_unique<views::MenuRunner>(model->model(),
                                              views::MenuRunner::CONTEXT_MENU);

  gfx::Point screen_point = browser->GetScreenPoint(
      gfx::Point(params.x, params.y), /*want_dip_coords=*/true);

  menu_->RunMenuAt(widget, nullptr, gfx::Rect(screen_point, gfx::Size()),
                   views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE,
                   /*native_view_for_gestures=*/nullptr, parent_widget);

  return true;
}

void CefMenuRunnerViewsAura::CancelContextMenu() {
  if (menu_) {
    menu_->Cancel();
  }
}

bool CefMenuRunnerViewsAura::FormatLabel(std::u16string& label) {
  // Remove the accelerator indicator (&) from label strings.
  const std::u16string::value_type replace[] = {u'&', 0};
  return base::ReplaceChars(label, replace, std::u16string(), &label);
}
