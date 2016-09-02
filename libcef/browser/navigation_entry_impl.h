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

// CefNavigationEntry implementation
class CefNavigationEntryImpl
    : public CefValueBase<CefNavigationEntry, content::NavigationEntry> {
 public:
  explicit CefNavigationEntryImpl(content::NavigationEntry* value);

  // CefNavigationEntry methods.
  bool IsValid() override;
  CefString GetURL() override;
  CefString GetDisplayURL() override;
  CefString GetOriginalURL() override;
  CefString GetTitle() override;
  TransitionType GetTransitionType() override;
  bool HasPostData() override;
  CefTime GetCompletionTime() override;
  int GetHttpStatusCode() override;
  CefRefPtr<CefSSLStatus> GetSSLStatus() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefNavigationEntryImpl);
};

#endif  // CEF_LIBCEF_BROWSER_NAVIGATION_ENTRY_IMPL_H_
