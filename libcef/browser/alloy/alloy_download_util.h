// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_UTIL_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_UTIL_H_
#pragma once

class DownloadPrefs;

namespace content {
class BrowserContext;
}  // namespace content

namespace alloy {

// Called from DownloadPrefs::FromBrowserContext.
DownloadPrefs* GetDownloadPrefsFromBrowserContext(
    content::BrowserContext* context);

}  // namespace alloy

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DOWNLOAD_UTIL_H_
