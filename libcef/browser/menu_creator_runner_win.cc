// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/menu_creator_runner_win.h"
#include "libcef/browser/browser_host_impl.h"

#include "base/message_loop.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/point.h"
#include "ui/views/controls/menu/menu_2.h"

CefMenuCreatorRunnerWin::CefMenuCreatorRunnerWin() {
}

bool CefMenuCreatorRunnerWin::RunContextMenu(CefMenuCreator* manager) {
  // Create a menu based on the model.
  menu_.reset(new views::NativeMenuWin(manager->model(), NULL));
  menu_->Rebuild();

  // Make sure events can be pumped while the menu is up.
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

  gfx::Point screen_point;

  if (manager->browser()->IsWindowRenderingDisabled()) {
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
    POINT temp = {manager->params().x, manager->params().y};
    HWND hwnd =
        manager->browser()->GetWebContents()->GetView()->GetNativeView();
    ClientToScreen(hwnd, &temp);
    screen_point = temp;
  }

  // Show the menu. Blocks until the menu is dismissed.
  menu_->RunMenuAt(screen_point, views::Menu2::ALIGN_TOPLEFT);

  return true;
}
