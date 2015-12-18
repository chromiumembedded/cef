// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
#define CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
#pragma once

#include "include/cef_ssl_info.h"

#include "net/ssl/ssl_info.h"

// CefSSLInfo implementation
class CefSSLInfoImpl : public CefSSLInfo {
 public:
  explicit CefSSLInfoImpl(const net::SSLInfo& value);

  // CefSSLInfo methods.
  cef_cert_status_t GetCertStatus() override;
  bool IsCertStatusError() override;
  bool IsCertStatusMinorError() override;
  CefRefPtr<CefSSLCertPrincipal> GetSubject() override;
  CefRefPtr<CefSSLCertPrincipal> GetIssuer() override;
  CefRefPtr<CefBinaryValue> GetSerialNumber() override;
  CefTime GetValidStart() override;
  CefTime GetValidExpiry() override;
  CefRefPtr<CefBinaryValue> GetDEREncoded() override;
  CefRefPtr<CefBinaryValue> GetPEMEncoded() override;
  size_t GetIssuerChainSize() override;
  void GetDEREncodedIssuerChain(IssuerChainBinaryList& chain) override;
  void GetPEMEncodedIssuerChain(IssuerChainBinaryList& chain) override;

 private:
  cef_cert_status_t cert_status_;
  CefRefPtr<CefSSLCertPrincipal> subject_;
  CefRefPtr<CefSSLCertPrincipal> issuer_;
  CefRefPtr<CefBinaryValue> serial_number_;
  CefTime valid_start_;
  CefTime valid_expiry_;
  CefRefPtr<CefBinaryValue> der_encoded_;
  CefRefPtr<CefBinaryValue> pem_encoded_;
  IssuerChainBinaryList der_encoded_issuer_chain_;
  IssuerChainBinaryList pem_encoded_issuer_chain_;

  IMPLEMENT_REFCOUNTING(CefSSLInfoImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSSLInfoImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
