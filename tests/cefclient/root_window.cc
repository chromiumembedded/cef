// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/root_window.h"

#include "cefclient/main_context.h"
#include "cefclient/root_window_manager.h"

namespace client {

// static
scoped_refptr<RootWindow> GetForBrowser(int browser_id) {
  return MainContext::Get()->GetRootWindowManager()->GetWindowForBrowser(
      browser_id);
}

}  // namespace client
