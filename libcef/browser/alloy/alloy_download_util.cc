// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/alloy/alloy_download_util.h"

#include "cef/libcef/browser/alloy/alloy_browser_context.h"

namespace alloy {

DownloadPrefs* GetDownloadPrefsFromBrowserContext(
    content::BrowserContext* context) {
  // This function is only called with Alloy bootstrap, so the static_cast is
  // safe.
  return static_cast<AlloyBrowserContext*>(context)->GetDownloadPrefs();
}

}  // namespace alloy
