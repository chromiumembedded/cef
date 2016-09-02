// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_status_impl.h"

#include "libcef/browser/x509_certificate_impl.h"

#include "content/public/browser/cert_store.h"
#include "net/ssl/ssl_connection_status_flags.h"

CefSSLStatusImpl::CefSSLStatusImpl(const content::SSLStatus& value) {
  cert_status_ = static_cast<cef_cert_status_t>(value.cert_status);
  content_status_ = static_cast<cef_ssl_content_status_t>(value.content_status);
  ssl_version_ = static_cast<cef_ssl_version_t>(
      net::SSLConnectionStatusToVersion(value.connection_status));
  cert_id_ = value.cert_id;
}

bool CefSSLStatusImpl::IsSecureConnection() {
  // Secure connection if there was a certificate ID in SSLStatus.
  return (cert_id_ != 0);
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
  if (cert_id_) {
    scoped_refptr<net::X509Certificate> cert;
    content::CertStore::GetInstance()->RetrieveCert(cert_id_, &cert);    
    if (cert.get())
      return new CefX509CertificateImpl(*cert);
  }
  return nullptr;
}
