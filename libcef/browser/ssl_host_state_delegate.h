// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SSL_HOST_STATE_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_SSL_HOST_STATE_DELEGATE_H_

#include <map>
#include <string>

#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

// Implementation based on android_webview/browser/aw_ssl_host_state_delegate.h.

namespace internal {

// This class maintains the policy for storing actions on certificate errors.
class CertPolicy {
 public:
  CertPolicy();
  ~CertPolicy();
  // Returns true if the user has decided to proceed through the ssl error
  // before. For a certificate to be allowed, it must not have any
  // *additional* errors from when it was allowed.
  bool Check(const net::X509Certificate& cert, net::CertStatus error) const;

  // Causes the policy to allow this certificate for a given |error|. And
  // remember the user's choice.
  void Allow(const net::X509Certificate& cert, net::CertStatus error);

  // Returns true if and only if there exists a user allow exception for some
  // certificate.
  bool HasAllowException() const { return allowed_.size() > 0; }

 private:
  // The set of fingerprints of allowed certificates.
  std::map<net::SHA256HashValue, net::CertStatus, net::SHA256HashValueLessThan>
      allowed_;
};

}  // namespace internal

class CefSSLHostStateDelegate : public content::SSLHostStateDelegate {
 public:
  CefSSLHostStateDelegate();
  ~CefSSLHostStateDelegate() override;

  // SSLHostStateDelegate methods:
  void AllowCert(const std::string& host,
                 const net::X509Certificate& cert,
                 net::CertStatus error) override;
  void Clear(
      const base::Callback<bool(const std::string&)>& host_filter) override;
  content::SSLHostStateDelegate::CertJudgment QueryPolicy(
      const std::string& host,
      const net::X509Certificate& cert,
      net::CertStatus error,
      bool* expired_previous_decision) override;
  void HostRanInsecureContent(const std::string& host,
                              int child_id,
                              InsecureContentType content_type) override;
  bool DidHostRunInsecureContent(
      const std::string& host,
      int child_id,
      InsecureContentType content_type) const override;
  void RevokeUserAllowExceptions(const std::string& host) override;
  bool HasAllowException(const std::string& host) const override;

 private:
  // Certificate policies for each host.
  std::map<std::string, internal::CertPolicy> cert_policy_for_host_;

  DISALLOW_COPY_AND_ASSIGN(CefSSLHostStateDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_SSL_HOST_STATE_DELEGATE_H_
