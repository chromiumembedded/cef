// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/chrome_browser_context.h"

#include "chrome/browser/profiles/profile_manager.h"

ChromeBrowserContext::ChromeBrowserContext(
    const CefRequestContextSettings& settings)
    : CefBrowserContext(settings) {}

ChromeBrowserContext::~ChromeBrowserContext() = default;

content::BrowserContext* ChromeBrowserContext::AsBrowserContext() {
  return profile_;
}

Profile* ChromeBrowserContext::AsProfile() {
  return profile_;
}

void ChromeBrowserContext::Initialize() {
  CefBrowserContext::Initialize();

  // TODO(chrome-runtime): ProfileManager can create new profiles relative to
  // the user-data-dir, but it should be done asynchronously.
  // The global ProfileManager instance can be retrieved via
  // |g_browser_process->profile_manager()|.
  profile_ = ProfileManager::GetLastUsedProfileAllowedByPolicy();
}

void ChromeBrowserContext::Shutdown() {
  CefBrowserContext::Shutdown();
  profile_ = nullptr;
}
