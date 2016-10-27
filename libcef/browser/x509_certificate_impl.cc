// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/x509_certificate_impl.h"
#include "libcef/browser/x509_cert_principal_impl.h"
#include "libcef/common/time_util.h"

namespace {

CefRefPtr<CefBinaryValue> EncodeCertificate(
    const net::X509Certificate::OSCertHandle& os_handle, bool der) {
  CefRefPtr<CefBinaryValue> bin_encoded;
  std::string encoded;

  if (( der && net::X509Certificate::GetDEREncoded(os_handle, &encoded)) ||
      (!der && net::X509Certificate::GetPEMEncoded(os_handle, &encoded))) {
    bin_encoded = CefBinaryValue::Create(encoded.c_str(), encoded.size());
  }

  return bin_encoded;
}

}  // namespace

CefX509CertificateImpl::CefX509CertificateImpl(
    scoped_refptr<net::X509Certificate> cert)
  :cert_(cert) {
}

CefRefPtr<CefX509CertPrincipal> CefX509CertificateImpl::GetSubject() {
  if (cert_)
    return new CefX509CertPrincipalImpl(cert_->subject());
  return nullptr;
}

CefRefPtr<CefX509CertPrincipal> CefX509CertificateImpl::GetIssuer() {
  if (cert_)
    return new CefX509CertPrincipalImpl(cert_->issuer());
  return nullptr;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetSerialNumber() {
  if (cert_) {
    const std::string& serial = cert_->serial_number();
    return CefBinaryValue::Create(serial.c_str(), serial.size());
  }
  return nullptr;
}

CefTime CefX509CertificateImpl::GetValidStart() {
  CefTime validity;
  if (cert_) {
    const base::Time& valid_time = cert_->valid_start();
    if (!valid_time.is_null())
      cef_time_from_basetime(valid_time, validity);
  }
  return validity;
}

CefTime CefX509CertificateImpl::GetValidExpiry() {
  CefTime validity;
  if (cert_) {
    const base::Time& valid_time = cert_->valid_expiry();
    if (!valid_time.is_null())
      cef_time_from_basetime(valid_time, validity);
  }
  return validity;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetDEREncoded() {
  if (cert_) {
    net::X509Certificate::OSCertHandle os_handle = cert_->os_cert_handle();
    if (os_handle)
      return EncodeCertificate(os_handle, true);
  }
  return nullptr;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetPEMEncoded() {
  if (cert_) {
    net::X509Certificate::OSCertHandle os_handle = cert_->os_cert_handle();
    if (os_handle)
      return EncodeCertificate(os_handle, false);
  }
  return nullptr;
}

size_t CefX509CertificateImpl::GetIssuerChainSize() {
  if (cert_)
    return cert_->GetIntermediateCertificates().size();
  return 0;
}

void CefX509CertificateImpl::GetEncodedIssuerChain(
    CefX509Certificate::IssuerChainBinaryList& chain, bool der) {
  chain.clear();
  if (cert_) {
    const net::X509Certificate::OSCertHandles& handles =
        cert_->GetIntermediateCertificates();
    for (net::X509Certificate::OSCertHandles::const_iterator it =
        handles.begin(); it != handles.end(); it++) {
      // Add each to the chain, even if one conversion unexpectedly failed.
      // GetIssuerChainSize depends on these being the same length.
      chain.push_back(EncodeCertificate(*it, der));
    }
  }
}

void CefX509CertificateImpl::GetDEREncodedIssuerChain(
    CefX509Certificate::IssuerChainBinaryList& chain) {
  if (der_encoded_issuer_chain_.empty())
    GetEncodedIssuerChain(der_encoded_issuer_chain_, true);
  chain = der_encoded_issuer_chain_;
}

void CefX509CertificateImpl::GetPEMEncodedIssuerChain(
    CefX509Certificate::IssuerChainBinaryList& chain) {
  if (pem_encoded_issuer_chain_.empty())
    GetEncodedIssuerChain(pem_encoded_issuer_chain_, false);
  chain = pem_encoded_issuer_chain_;
}
