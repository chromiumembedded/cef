// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_security_policy_impl.h"

#include "cef/libcef/browser/thread_util.h"

namespace {

void RunSecurityCallback(CefRefPtr<CefBrowserSecurityCallback> callback) {
  if (!callback) {
    return;
  }
  callback->OnComplete(
      true,
      "Browser security settings were updated. Enforcement is not yet "
      "implemented in this scaffold.");
}

}  // namespace

CefBrowserSecurityPolicyImpl::CefBrowserSecurityPolicyImpl() = default;

CefBrowserSecuritySettings CefBrowserSecurityPolicyImpl::GetSettings() {
  base::AutoLock lock_scope(lock_);
  return settings_;
}

void CefBrowserSecurityPolicyImpl::SetSettings(
    const CefBrowserSecuritySettings& settings,
    CefRefPtr<CefBrowserSecurityCallback> callback) {
  {
    base::AutoLock lock_scope(lock_);
    settings_ = settings;
  }

  if (!callback) {
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&RunSecurityCallback, callback));
    return;
  }

  RunSecurityCallback(callback);
}

CefString CefBrowserSecurityPolicyImpl::GetAllowedDomains() {
  base::AutoLock lock_scope(lock_);
  return CefString(&settings_.allowed_domains);
}

CefString CefBrowserSecurityPolicyImpl::GetConfirmActionCategories() {
  base::AutoLock lock_scope(lock_);
  return CefString(&settings_.confirm_actions);
}
