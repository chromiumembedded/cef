// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "cef/include/cef_browser_security.h"
#include "url/gurl.h"

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

  // Fast domain check against pre-compiled allowed domains list.
  // Returns true if the domain is allowed (or if no domain restriction is set).
  bool IsDomainAllowed(const std::string& domain) const;

  // Check if a request URL should be blocked by policy.
  // This is the fast pre-check used before expensive request setup.
  bool ShouldBlockRequest(const GURL& url) const;

 private:
  ~CefBrowserSecurityPolicyImpl() override = default;

  // Pre-compiled domain matchers for fast allow/deny checks.
  struct CompiledPolicy {
    std::vector<std::string> allowed_domains;
    std::vector<std::string> confirm_action_categories;
    bool has_allowed_domains = false;
    bool has_confirm_actions = false;
  };

  // Recompile the policy from raw settings. Called on SetSettings().
  void RecompilePolicy();

  mutable base::Lock lock_;
  CefBrowserSecuritySettings settings_;
  CompiledPolicy compiled_policy_;

  IMPLEMENT_REFCOUNTING(CefBrowserSecurityPolicyImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_SECURITY_POLICY_IMPL_H_
