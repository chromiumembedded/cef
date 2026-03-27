// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_security_policy_impl.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cef/libcef/browser/thread_util.h"
#include "url/gurl.h"

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

void CefBrowserSecurityPolicyImpl::GetSettings(
    CefBrowserSecuritySettings& settings) {
  base::AutoLock lock_scope(lock_);
  settings = settings_;
}

void CefBrowserSecurityPolicyImpl::SetSettings(
    const CefBrowserSecuritySettings& settings,
    CefRefPtr<CefBrowserSecurityCallback> callback) {
  {
    base::AutoLock lock_scope(lock_);
    settings_ = settings;
    RecompilePolicy();
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

void CefBrowserSecurityPolicyImpl::RecompilePolicy() {
  lock_.AssertAcquired();

  compiled_policy_ = CompiledPolicy();

  const std::string allowed_domains_str =
      CefString(&settings_.allowed_domains).ToString();
  if (!allowed_domains_str.empty()) {
    compiled_policy_.allowed_domains = base::SplitString(
        allowed_domains_str, ",", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    compiled_policy_.has_allowed_domains =
        !compiled_policy_.allowed_domains.empty();
  }

  const std::string confirm_actions_str =
      CefString(&settings_.confirm_actions).ToString();
  if (!confirm_actions_str.empty()) {
    compiled_policy_.confirm_action_categories = base::SplitString(
        confirm_actions_str, ",", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    compiled_policy_.has_confirm_actions =
        !compiled_policy_.confirm_action_categories.empty();
  }
}

bool CefBrowserSecurityPolicyImpl::IsDomainAllowed(
    const std::string& domain) const {
  base::AutoLock lock_scope(lock_);

  if (!compiled_policy_.has_allowed_domains) {
    return true;
  }

  for (const auto& allowed : compiled_policy_.allowed_domains) {
    if (allowed.empty()) {
      continue;
    }

    if (allowed[0] == '.') {
      // Wildcard prefix match: ".example.com" matches "sub.example.com"
      // and "example.com" itself.
      const std::string suffix = allowed;  // e.g., ".example.com"
      const std::string bare = allowed.substr(1);  // e.g., "example.com"

      if (base::EqualsCaseInsensitiveASCII(domain, bare)) {
        return true;
      }
      if (domain.size() > suffix.size() &&
          base::EndsWith(domain, suffix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        return true;
      }
    } else {
      // Exact match.
      if (base::EqualsCaseInsensitiveASCII(domain, allowed)) {
        return true;
      }
    }
  }

  return false;
}

bool CefBrowserSecurityPolicyImpl::ShouldBlockRequest(
    const GURL& url) const {
  base::AutoLock lock_scope(lock_);

  if (!compiled_policy_.has_allowed_domains) {
    return false;
  }

  // Release the lock and call IsDomainAllowed which acquires it.
  // Instead, inline the domain check here under the same lock.
  const std::string host = url.host();

  for (const auto& allowed : compiled_policy_.allowed_domains) {
    if (allowed.empty()) {
      continue;
    }

    if (allowed[0] == '.') {
      const std::string suffix = allowed;
      const std::string bare = allowed.substr(1);

      if (base::EqualsCaseInsensitiveASCII(host, bare)) {
        return false;
      }
      if (host.size() > suffix.size() &&
          base::EndsWith(host, suffix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        return false;
      }
    } else {
      if (base::EqualsCaseInsensitiveASCII(host, allowed)) {
        return false;
      }
    }
  }

  // Domain not in allowed list, block the request.
  return true;
}
