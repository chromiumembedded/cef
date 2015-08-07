// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_creator_runner_win.h"
#include "libcef/browser/browser_host_impl.h"

#include "base/message_loop/message_loop.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/menu/menu_2.h"

CefMenuCreatorRunnerWin::CefMenuCreatorRunnerWin() {
}

bool CefMenuCreatorRunnerWin::RunContextMenu(CefMenuCreator* manager) {
  // Create a menu based on the model.
  menu_.reset(new views::NativeMenuWin(manager->model(), NULL));
  menu_->Rebuild(NULL);

  // Make sure events can be pumped while the menu is up.
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());

  gfx::Point screen_point;

  if (manager->browser()->IsWindowless()) {
    CefRefPtr<CefClient> client = manager->browser()->GetClient();
    if (!client.get())
      return false;

    CefRefPtr<CefRenderHandler> handler = client->GetRenderHandler();
    if (!handler.get())
      return false;

    int screenX = 0, screenY = 0;
    if (!handler->GetScreenPoint(manager->browser(),
                                 manager->params().x, manager->params().y,
                                 screenX, screenY)) {
      return false;
    }

    screen_point = gfx::Point(screenX, screenY);
  } else {
    aura::Window* window = manager->browser()->GetContentView();
    const gfx::Rect& bounds_in_screen = window->GetBoundsInScreen();
    screen_point = gfx::Point(bounds_in_screen.x() + manager->params().x,
                              bounds_in_screen.y() + manager->params().y);

    // Adjust for potential display scaling.
    float scale = gfx::Screen::GetScreenFor(window)->
        GetDisplayNearestWindow(window).device_scale_factor();
    screen_point = gfx::ToFlooredPoint(gfx::ScalePoint(screen_point, scale));
  }

  // Show the menu. Blocks until the menu is dismissed.
  menu_->RunMenuAt(screen_point, views::Menu2::ALIGN_TOPLEFT);

  return true;
}
