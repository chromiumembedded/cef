// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/ssl_cert_principal_impl.h"
#include "libcef/common/time_util.h"

#include "net/cert/x509_certificate.h"

CefSSLInfoImpl::CefSSLInfoImpl(const net::SSLInfo& value) {
  if (value.cert.get()) {
    subject_ = new CefSSLCertPrincipalImpl(value.cert->subject());
    issuer_ = new CefSSLCertPrincipalImpl(value.cert->issuer());

    const std::string& serial_number = value.cert->serial_number();
    serial_number_ = CefBinaryValue::Create(serial_number.c_str(),
                                            serial_number.size());

    const base::Time& valid_start = value.cert->valid_start();
    if (!valid_start.is_null())
      cef_time_from_basetime(valid_start, valid_start_);

    const base::Time& valid_expiry = value.cert->valid_expiry();
    if (!valid_expiry.is_null())
      cef_time_from_basetime(valid_expiry, valid_expiry_);

    net::X509Certificate::OSCertHandle os_handle = value.cert->os_cert_handle();
    if (os_handle) {
      std::string encoded;
      if (value.cert->GetDEREncoded(os_handle, &encoded)) {
        der_encoded_ = CefBinaryValue::Create(encoded.c_str(),
                                              encoded.size());
      }
      encoded.clear();
      if (value.cert->GetPEMEncoded(os_handle, &encoded)) {
        pem_encoded_ = CefBinaryValue::Create(encoded.c_str(),
                                              encoded.size());
      }
    }
  }
}

CefRefPtr<CefSSLCertPrincipal> CefSSLInfoImpl::GetSubject() {
  return subject_;
}

CefRefPtr<CefSSLCertPrincipal> CefSSLInfoImpl::GetIssuer() {
  return issuer_;
}

CefRefPtr<CefBinaryValue> CefSSLInfoImpl::GetSerialNumber() {
  return serial_number_;
}

CefTime CefSSLInfoImpl::GetValidStart() {
  return valid_start_;
}

CefTime CefSSLInfoImpl::GetValidExpiry() {
  return valid_expiry_;
}

CefRefPtr<CefBinaryValue> CefSSLInfoImpl::GetDEREncoded() {
  return der_encoded_;
}

CefRefPtr<CefBinaryValue> CefSSLInfoImpl::GetPEMEncoded() {
  return pem_encoded_;
}
