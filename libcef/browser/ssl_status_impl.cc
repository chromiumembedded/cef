// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_status_impl.h"

#include "libcef/browser/x509_certificate_impl.h"

#include "net/ssl/ssl_connection_status_flags.h"

CefSSLStatusImpl::CefSSLStatusImpl(const content::SSLStatus& value) {
  cert_status_ = static_cast<cef_cert_status_t>(value.cert_status);
  content_status_ = static_cast<cef_ssl_content_status_t>(value.content_status);
  ssl_version_ = static_cast<cef_ssl_version_t>(
      net::SSLConnectionStatusToVersion(value.connection_status));
  certificate_ = value.certificate;
}

bool CefSSLStatusImpl::IsSecureConnection() {
  return !!certificate_.get();
}

cef_cert_status_t CefSSLStatusImpl::GetCertStatus() {
  return cert_status_;
}

cef_ssl_version_t CefSSLStatusImpl::GetSSLVersion() {
  return ssl_version_;
}

cef_ssl_content_status_t CefSSLStatusImpl::GetContentStatus() {
  return content_status_;
}

CefRefPtr<CefX509Certificate> CefSSLStatusImpl::GetX509Certificate() {
  if (certificate_ && !cef_certificate_)
    cef_certificate_ = new CefX509CertificateImpl(certificate_);
  return cef_certificate_;
}
