// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
#pragma once

#include "base/synchronization/lock.h"
#include "cef/include/cef_browser_security.h"

// Implementation of the CefBrowserSecurityPolicy interface.
class CefBrowserSecurityPolicyImpl : public CefBrowserSecurityPolicy {
 public:
  CefBrowserSecurityPolicyImpl();

  CefBrowserSecurityPolicyImpl(const CefBrowserSecurityPolicyImpl&) = delete;
  CefBrowserSecurityPolicyImpl& operator=(const CefBrowserSecurityPolicyImpl&) =
      delete;

  void GetSettings(CefBrowserSecuritySettings& settings) override;
  void SetSettings(const CefBrowserSecuritySettings& settings,
                   CefRefPtr<CefBrowserSecurityCallback> callback) override;
  CefString GetAllowedDomains() override;
  CefString GetConfirmActionCategories() override;

 private:
  ~CefBrowserSecurityPolicyImpl() override = default;

  base::Lock lock_;
  CefBrowserSecuritySettings settings_;

  IMPLEMENT_REFCOUNTING(CefBrowserSecurityPolicyImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
