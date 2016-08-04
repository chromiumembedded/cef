// Copyright (c) 2016 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_profile_manager_stub.h"

#include "libcef/browser/browser_context_impl.h"

ChromeProfileManagerStub::ChromeProfileManagerStub()
    : ProfileManager(base::FilePath()) {
}

ChromeProfileManagerStub::~ChromeProfileManagerStub() {
}

Profile* ChromeProfileManagerStub::GetProfile(
    const base::FilePath& profile_dir) {
  scoped_refptr<CefBrowserContextImpl> browser_context =
      CefBrowserContextImpl::GetForCachePath(profile_dir);
  DCHECK(browser_context);
  return browser_context.get();
}

bool ChromeProfileManagerStub::IsValidProfile(const void* profile) {
  if (!profile)
    return false;
  return !!CefBrowserContextImpl::GetForContext(
      reinterpret_cast<content::BrowserContext*>(
          const_cast<void*>(profile))).get();
}
