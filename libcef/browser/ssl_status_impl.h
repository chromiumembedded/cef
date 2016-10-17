// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SSL_STATUS_IMPL_H_
#define CEF_LIBCEF_BROWSER_SSL_STATUS_IMPL_H_
#pragma once

#include "include/cef_ssl_status.h"

#include "content/public/browser/ssl_status.h"

// CefSSLStatus implementation
class CefSSLStatusImpl : public CefSSLStatus {
 public:
  explicit CefSSLStatusImpl(const content::SSLStatus& value);

  // CefSSLStatus methods.
  bool IsSecureConnection() override;
  cef_cert_status_t GetCertStatus() override;
  cef_ssl_version_t GetSSLVersion() override;
  cef_ssl_content_status_t GetContentStatus() override;
  CefRefPtr<CefX509Certificate> GetX509Certificate() override;

 private:
  cef_cert_status_t cert_status_;
  cef_ssl_version_t ssl_version_;
  cef_ssl_content_status_t content_status_;

  // Don't create a CefX509Certificate object until requested.
  scoped_refptr<net::X509Certificate> certificate_;
  CefRefPtr<CefX509Certificate> cef_certificate_;

  IMPLEMENT_REFCOUNTING(CefSSLStatusImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSSLStatusImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SSL_STATUS_IMPL_H_
