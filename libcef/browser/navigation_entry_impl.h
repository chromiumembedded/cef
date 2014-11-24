// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NAVIGATION_ENTRY_IMPL_H_
#define CEF_LIBCEF_BROWSER_NAVIGATION_ENTRY_IMPL_H_
#pragma once

#include "include/cef_navigation_entry.h"
#include "libcef/common/value_base.h"

namespace content {
class NavigationEntry;
}

// CefDownloadItem implementation
class CefNavigationEntryImpl
    : public CefValueBase<CefNavigationEntry, content::NavigationEntry> {
 public:
  explicit CefNavigationEntryImpl(content::NavigationEntry* value);

  // CefNavigationEntry methods.
  bool IsValid() OVERRIDE;
  CefString GetURL() OVERRIDE;
  CefString GetDisplayURL() OVERRIDE;
  CefString GetOriginalURL() OVERRIDE;
  CefString GetTitle() OVERRIDE;
  TransitionType GetTransitionType() OVERRIDE;
  bool HasPostData() OVERRIDE;
  CefString GetFrameName() OVERRIDE;
  CefTime GetCompletionTime() OVERRIDE;
  int GetHttpStatusCode() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefNavigationEntryImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NAVIGATION_ENTRY_IMPL_H_
