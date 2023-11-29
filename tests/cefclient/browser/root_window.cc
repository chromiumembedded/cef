// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window.h"

#include "include/base/cef_callback_helpers.h"

#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/shared/common/client_switches.h"

namespace client {

RootWindowConfig::RootWindowConfig(CefRefPtr<CefCommandLine> cmd)
    : command_line(cmd ? cmd : MainContext::Get()->GetCommandLine()),
      with_controls(!command_line->HasSwitch(switches::kHideControls)),
      url(MainContext::Get()->GetMainURL(command_line)) {}

RootWindow::RootWindow() : delegate_(nullptr) {}

RootWindow::~RootWindow() {}

// static
scoped_refptr<RootWindow> RootWindow::GetForBrowser(int browser_id) {
  return MainContext::Get()->GetRootWindowManager()->GetWindowForBrowser(
      browser_id);
}

void RootWindow::OnExtensionsChanged(const ExtensionSet& extensions) {
  REQUIRE_MAIN_THREAD();
  DCHECK(delegate_);
  DCHECK(!WithExtension());

  if (extensions.empty()) {
    return;
  }

  ExtensionSet::const_iterator it = extensions.begin();
  for (; it != extensions.end(); ++it) {
    delegate_->CreateExtensionWindow(*it, CefRect(), nullptr, base::DoNothing(),
                                     WithWindowlessRendering());
  }
}

}  // namespace client
