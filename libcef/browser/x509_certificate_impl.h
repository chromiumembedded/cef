// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_X509_CERTIFICATE_IMPL_H_
#define CEF_LIBCEF_BROWSER_X509_CERTIFICATE_IMPL_H_
#pragma once

#include "include/cef_x509_certificate.h"

#include <memory>

#include "net/ssl/client_cert_identity.h"

// CefX509Certificate implementation
class CefX509CertificateImpl : public CefX509Certificate {
 public:
  explicit CefX509CertificateImpl(scoped_refptr<net::X509Certificate> cert);

  // Used with CefContentBrowserClient::SelectClientCertificate only.
  explicit CefX509CertificateImpl(
      std::unique_ptr<net::ClientCertIdentity> identity);

  // CefX509Certificate methods.
  CefRefPtr<CefX509CertPrincipal> GetSubject() override;
  CefRefPtr<CefX509CertPrincipal> GetIssuer() override;
  CefRefPtr<CefBinaryValue> GetSerialNumber() override;
  CefTime GetValidStart() override;
  CefTime GetValidExpiry() override;
  CefRefPtr<CefBinaryValue> GetDEREncoded() override;
  CefRefPtr<CefBinaryValue> GetPEMEncoded() override;
  size_t GetIssuerChainSize() override;
  void GetDEREncodedIssuerChain(IssuerChainBinaryList& chain) override;
  void GetPEMEncodedIssuerChain(IssuerChainBinaryList& chain) override;

  scoped_refptr<net::X509Certificate> GetInternalCertObject() { return cert_; }
  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback);

 private:
  void GetEncodedIssuerChain(IssuerChainBinaryList& chain, bool der);

  std::unique_ptr<net::ClientCertIdentity> identity_;
  scoped_refptr<net::X509Certificate> cert_;
  IssuerChainBinaryList pem_encoded_issuer_chain_;
  IssuerChainBinaryList der_encoded_issuer_chain_;

  IMPLEMENT_REFCOUNTING(CefX509CertificateImpl);
  DISALLOW_COPY_AND_ASSIGN(CefX509CertificateImpl);
};

#endif  // CEF_LIBCEF_BROWSER_X509_CERTIFICATE_IMPL_H_
