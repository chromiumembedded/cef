// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/menu_creator_runner_linux.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/window_x11.h"

#include "base/strings/string_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/point.h"

// Used to silence warnings about unused variables.
#if !defined(UNUSED)
#define UNUSED(x) ((void)(x))
#endif

CefMenuCreatorRunnerLinux::CefMenuCreatorRunnerLinux() {
}

CefMenuCreatorRunnerLinux::~CefMenuCreatorRunnerLinux() {
}

bool CefMenuCreatorRunnerLinux::RunContextMenu(CefMenuCreator* manager) {
  menu_.reset(new views::MenuRunner(manager->model()));

  // We can't use aura::Window::GetBoundsInScreen on Linux because it will
  // return bounds from DesktopWindowTreeHostX11 which in our case is relative
  // to the parent window instead of the root window (screen).
  const gfx::Rect& bounds_in_screen =
      manager->browser()->window_x11()->GetBoundsInScreen();
  gfx::Point screen_point =
      gfx::Point(bounds_in_screen.x() + manager->params().x,
                 bounds_in_screen.y() + manager->params().y);

  views::MenuRunner::RunResult result =
      menu_->RunMenuAt(manager->browser()->window_widget(),
                       NULL, gfx::Rect(screen_point, gfx::Size()),
                       views::MenuItemView::TOPRIGHT, ui::MENU_SOURCE_NONE,
                       views::MenuRunner::CONTEXT_MENU);
  UNUSED(result);

  return true;
}

bool CefMenuCreatorRunnerLinux::FormatLabel(base::string16& label) {
  // Remove the accelerator indicator (&) from label strings.
  const char16 replace[] = {L'&', 0};
  return base::ReplaceChars(label, replace, base::string16(), &label);
}

