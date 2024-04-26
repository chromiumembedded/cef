// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
#pragma once

#include <memory>

#include "content/public/browser/download_manager_delegate.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace cef {

class DownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  // Called from the ChromeDownloadManagerDelegate constructor for Chrome
  // bootstrap. Alloy bootstrap uses AlloyDownloadManagerDelegate directly.
  static std::unique_ptr<DownloadManagerDelegate> Create(
      content::DownloadManager* download_manager);

 private:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<DownloadManagerDelegate>;
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_DOWNLOAD_MANAGER_DELEGATE_H_
