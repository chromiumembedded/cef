// Copyright 2024 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_content_browser_client_cef.h"

#include "cef/libcef/browser/chrome/chrome_web_contents_view_delegate_cef.h"

// static
std::unique_ptr<content::WebContentsViewDelegate>
ChromeContentBrowserClientCef::CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return std::make_unique<ChromeWebContentsViewDelegateCef>(web_contents);
}
