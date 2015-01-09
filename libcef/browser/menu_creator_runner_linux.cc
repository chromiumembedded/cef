// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/menu_creator_runner_linux.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/window_x11.h"

#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"

CefMenuCreatorRunnerLinux::CefMenuCreatorRunnerLinux() {
}

CefMenuCreatorRunnerLinux::~CefMenuCreatorRunnerLinux() {
}

bool CefMenuCreatorRunnerLinux::RunContextMenu(CefMenuCreator* manager) {
  menu_.reset(
      new views::MenuRunner(manager->model(), views::MenuRunner::CONTEXT_MENU));

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
    // We can't use aura::Window::GetBoundsInScreen on Linux because it will
    // return bounds from DesktopWindowTreeHostX11 which in our case is relative
    // to the parent window instead of the root window (screen).
    const gfx::Rect& bounds_in_screen =
        manager->browser()->window_x11()->GetBoundsInScreen();
    screen_point = gfx::Point(bounds_in_screen.x() + manager->params().x,
                              bounds_in_screen.y() + manager->params().y);
  }

  views::MenuRunner::RunResult result =
      menu_->RunMenuAt(manager->browser()->window_widget(),
                       NULL, gfx::Rect(screen_point, gfx::Size()),
                       views::MENU_ANCHOR_TOPRIGHT,
                       ui::MENU_SOURCE_NONE);
  ALLOW_UNUSED_LOCAL(result);

  return true;
}

void CefMenuCreatorRunnerLinux::CancelContextMenu() {
  if (menu_)
    menu_->Cancel();
}

bool CefMenuCreatorRunnerLinux::FormatLabel(base::string16& label) {
  // Remove the accelerator indicator (&) from label strings.
  const char16 replace[] = {L'&', 0};
  return base::ReplaceChars(label, replace, base::string16(), &label);
}
