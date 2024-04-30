// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/download_manager_delegate.h"

#include "cef/libcef/browser/download_manager_delegate_impl.h"
#include "cef/libcef/features/runtime_checks.h"

namespace cef {

// static
std::unique_ptr<cef::DownloadManagerDelegate> DownloadManagerDelegate::Create(
    content::DownloadManager* download_manager) {
#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  REQUIRE_CHROME_RUNTIME();
#endif
  return std::make_unique<CefDownloadManagerDelegateImpl>(
      download_manager, /*alloy_bootstrap=*/false);
}

}  // namespace cef
