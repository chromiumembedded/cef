// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
#pragma once

#include "libcef/browser/browser_context.h"

// See CefBrowserContext documentation for usage. Only accessed on the UI thread
// unless otherwise indicated.
class ChromeBrowserContext : public CefBrowserContext {
 public:
  explicit ChromeBrowserContext(const CefRequestContextSettings& settings);

  // CefBrowserContext overrides.
  content::BrowserContext* AsBrowserContext() override;
  Profile* AsProfile() override;
  void Initialize() override;
  void Shutdown() override;

 private:
  ~ChromeBrowserContext() override;

  Profile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserContext);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_CONTEXT_H_
