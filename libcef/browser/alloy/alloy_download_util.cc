// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/alloy/alloy_download_util.h"

#include "libcef/browser/alloy/alloy_browser_context.h"

namespace alloy {

DownloadPrefs* GetDownloadPrefsFromBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AlloyBrowserContext*>(context)->GetDownloadPrefs();
}

}  // namespace alloy
