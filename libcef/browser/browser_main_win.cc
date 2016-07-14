// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <commctrl.h>
#include <Objbase.h>

#include "libcef/browser/browser_main.h"

#include "cef/grit/cef_strings.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/win/direct_write.h"

void CefBrowserMainParts::PlatformInitialize() {
  HRESULT res;

  // Initialize common controls.
  res = CoInitialize(NULL);
  DCHECK(SUCCEEDED(res));
  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Start COM stuff.
  res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));
}
