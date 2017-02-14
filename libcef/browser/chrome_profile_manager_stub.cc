// Copyright (c) 2016 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_profile_manager_stub.h"

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/content_browser_client.h"

namespace {

// Return the active browser context. This is primarily called from Chrome code
// that handles WebUI views and wishes to associate the view's data with a
// particular context (profile). Chrome stores multiple profiles in sub-
// directories of |user_data_dir| and then uses ProfileManager to track which
// profile (sub-directory name) was last active.
//
// TODO(cef): To most closely match Chrome behavior this should return the
// context for the currently active browser (e.g. the browser with input focus).
// Return the main context for now since we don't currently have a good way to
// determine that.
CefBrowserContextImpl* GetActiveBrowserContext() {
  return CefContentBrowserClient::Get()->browser_context();
}

}  // namespace

ChromeProfileManagerStub::ChromeProfileManagerStub()
    : ProfileManager(base::FilePath()) {
}

ChromeProfileManagerStub::~ChromeProfileManagerStub() {
}

Profile* ChromeProfileManagerStub::GetProfile(
    const base::FilePath& profile_dir) {
  CefBrowserContextImpl* browser_context =
      CefBrowserContextImpl::GetForCachePath(profile_dir);
  if (!browser_context) {
    // ProfileManager makes assumptions about profile directory paths that do
    // not match CEF usage. For example, the default Chrome profile name is
    // "Default" so it will append that sub-directory name to an empty
    // |user_data_dir| value and then call this method. Use the active context
    // in cases such as this where we don't understand what ProfileManager is
    // asking for.
    browser_context = GetActiveBrowserContext();
  }
  return browser_context;
}

bool ChromeProfileManagerStub::IsValidProfile(const void* profile) {
  if (!profile)
    return false;
  return !!CefBrowserContextImpl::GetForContext(
      reinterpret_cast<content::BrowserContext*>(
          const_cast<void*>(profile)));
}

Profile* ChromeProfileManagerStub::GetLastUsedProfile(
    const base::FilePath& user_data_dir) {
  // Override this method to avoid having to register prefs::kProfileLastUsed,
  // usage of which doesn't make sense for CEF.
  return GetActiveBrowserContext();
}
