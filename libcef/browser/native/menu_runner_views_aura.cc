// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/native/menu_runner_views_aura.h"

#include <memory>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/display/screen.h"
#include "ui/gfx/x/connection.h"
#endif

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

#if BUILDFLAG(IS_LINUX)
  if (browser->IsWindowless() && parent_widget) {
    // On Linux/X11 with OSR, menus are displayed as top-level override-redirect
    // windows parented to the root window (to avoid clipping). The menu system
    // expects screen coordinates in this case. GetScreenPoint returns
    // coordinates relative to the parent window, so we need to convert to
    // screen coordinates by adding the parent window's screen origin.
    auto* connection = x11::Connection::Get();
    if (connection) {
      if (auto reply = connection
                           ->TranslateCoordinates(
                               {static_cast<x11::Window>(parent_widget),
                                connection->default_root(), 0, 0})
                           .Sync()) {
        // TranslateCoordinates returns pixel coordinates, but screen_point is
        // in DIP coordinates. Convert the offset to DIPs using the display's
        // scale factor.
        gfx::Point origin_px(reply->dst_x, reply->dst_y);
        float scale = display::Screen::Get()
                          ->GetDisplayNearestPoint(origin_px)
                          .device_scale_factor();
        screen_point.Offset(static_cast<int>(reply->dst_x / scale),
                            static_cast<int>(reply->dst_y / scale));
      }
    }
  }
#endif

  menu_->RunMenuAt(widget, nullptr, gfx::Rect(screen_point, gfx::Size()),
                   views::MenuAnchorPosition::kTopRight,
                   ui::mojom::MenuSourceType::kNone,
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
