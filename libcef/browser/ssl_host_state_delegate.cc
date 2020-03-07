// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/ssl_host_state_delegate.h"

#include "base/callback.h"
#include "net/base/hash_value.h"

using content::SSLHostStateDelegate;

namespace internal {

CertPolicy::CertPolicy() {}
CertPolicy::~CertPolicy() {}

// For an allowance, we consider a given |cert| to be a match to a saved
// allowed cert if the |error| is an exact match to or subset of the errors
// in the saved CertStatus.
bool CertPolicy::Check(const net::X509Certificate& cert, int error) const {
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  const auto& allowed_iter = allowed_.find(fingerprint);
  if ((allowed_iter != allowed_.end()) && (allowed_iter->second & error) &&
      ((allowed_iter->second & error) == error)) {
    return true;
  }
  return false;
}

void CertPolicy::Allow(const net::X509Certificate& cert, int error) {
  // If this same cert had already been saved with a different error status,
  // this will replace it with the new error status.
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  allowed_[fingerprint] = error;
}

}  // namespace internal

CefSSLHostStateDelegate::CefSSLHostStateDelegate() {}

CefSSLHostStateDelegate::~CefSSLHostStateDelegate() {}

void CefSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  // Intentional no-op.
}

bool CefSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  // Intentional no-op.
  return false;
}

void CefSSLHostStateDelegate::AllowCert(const std::string& host,
                                        const net::X509Certificate& cert,
                                        int error,
                                        content::WebContents* web_contents) {
  cert_policy_for_host_[host].Allow(cert, error);
}

void CefSSLHostStateDelegate::Clear(
    const base::RepeatingCallback<bool(const std::string&)> host_filter) {
  if (host_filter.is_null()) {
    cert_policy_for_host_.clear();
    return;
  }

  for (auto it = cert_policy_for_host_.begin();
       it != cert_policy_for_host_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(it->first))
      cert_policy_for_host_.erase(it);

    it = next_it;
  }
}

SSLHostStateDelegate::CertJudgment CefSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::WebContents* web_contents) {
  return cert_policy_for_host_[host].Check(cert, error)
             ? SSLHostStateDelegate::ALLOWED
             : SSLHostStateDelegate::DENIED;
}

void CefSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  cert_policy_for_host_.erase(host);
}

bool CefSSLHostStateDelegate::HasAllowException(
    const std::string& host,
    content::WebContents* web_contents) {
  auto policy_iterator = cert_policy_for_host_.find(host);
  return policy_iterator != cert_policy_for_host_.end() &&
         policy_iterator->second.HasAllowException();
}
