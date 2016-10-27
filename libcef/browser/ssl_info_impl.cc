// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/x509_certificate_impl.h"

#include "net/cert/cert_status_flags.h"

CefSSLInfoImpl::CefSSLInfoImpl(const net::SSLInfo& value)
    : cert_status_(CERT_STATUS_NONE) {
  cert_status_ = static_cast<cef_cert_status_t>(value.cert_status);
  if (value.cert.get()) {
    cert_ = new CefX509CertificateImpl(value.cert);
  }
}

cef_cert_status_t CefSSLInfoImpl::GetCertStatus() {
  return cert_status_;
}

CefRefPtr<CefX509Certificate> CefSSLInfoImpl::GetX509Certificate() {
  return cert_;
}

bool CefIsCertStatusError(cef_cert_status_t status) {
  return net::IsCertStatusError(status);
}

bool CefIsCertStatusMinorError(cef_cert_status_t status) {
  return net::IsCertStatusMinorError(status);
}
