// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/public/browser/browser_context.h"

// Used by chrome/browser/spellchecker/spellcheck_factory.cc.
namespace chrome {

// Returns the original browser context even for Incognito contexts.
content::BrowserContext* GetBrowserContextRedirectedInIncognito(
    content::BrowserContext* context) {
  return context;
}

// Returns non-NULL even for Incognito contexts so that a separate
// instance of a service is created for the Incognito context.
content::BrowserContext* GetBrowserContextOwnInstanceInIncognito(
    content::BrowserContext* context) {
  return context;
}

}  // namespace chrome
