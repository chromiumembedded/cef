// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/x509_certificate_impl.h"
#include "libcef/browser/x509_cert_principal_impl.h"
#include "libcef/common/time_util.h"

namespace {

void EncodeCertificate(
    const net::X509Certificate::OSCertHandle& os_handle,
    CefRefPtr<CefBinaryValue>& der_encoded,
    CefRefPtr<CefBinaryValue>& pem_encoded) {
  std::string encoded;
  if (net::X509Certificate::GetDEREncoded(os_handle, &encoded)) {
    der_encoded = CefBinaryValue::Create(encoded.c_str(),
                                         encoded.size());
  }
  encoded.clear();
  if (net::X509Certificate::GetPEMEncoded(os_handle, &encoded)) {
    pem_encoded = CefBinaryValue::Create(encoded.c_str(),
                                         encoded.size());
  }
}

}  // namespace

CefX509CertificateImpl::CefX509CertificateImpl(
    const net::X509Certificate& value) {
  subject_ = new CefX509CertPrincipalImpl(value.subject());
  issuer_ = new CefX509CertPrincipalImpl(value.issuer());

  const std::string& serial_number = value.serial_number();
  serial_number_ = CefBinaryValue::Create(serial_number.c_str(),
                                          serial_number.size());

  const base::Time& valid_start = value.valid_start();
  if (!valid_start.is_null())
    cef_time_from_basetime(valid_start, valid_start_);

  const base::Time& valid_expiry = value.valid_expiry();
  if (!valid_expiry.is_null())
    cef_time_from_basetime(valid_expiry, valid_expiry_);

  net::X509Certificate::OSCertHandle os_handle = value.os_cert_handle();
  if (os_handle)
    EncodeCertificate(os_handle, der_encoded_, pem_encoded_);

  const net::X509Certificate::OSCertHandles& issuer_chain =
    value.GetIntermediateCertificates();
  for (net::X509Certificate::OSCertHandles::const_iterator it =
       issuer_chain.begin(); it != issuer_chain.end(); it++) {
    CefRefPtr<CefBinaryValue> der_encoded, pem_encoded;
    EncodeCertificate(*it, der_encoded, pem_encoded);

    // Add each to the chain, even if one conversion unexpectedly failed.
    // GetIssuerChainSize depends on these being the same length.
    der_encoded_issuer_chain_.push_back(der_encoded);
    pem_encoded_issuer_chain_.push_back(pem_encoded);
  }
}

CefRefPtr<CefX509CertPrincipal> CefX509CertificateImpl::GetSubject() {
  return subject_;
}

CefRefPtr<CefX509CertPrincipal> CefX509CertificateImpl::GetIssuer() {
  return issuer_;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetSerialNumber() {
  return serial_number_;
}

CefTime CefX509CertificateImpl::GetValidStart() {
  return valid_start_;
}

CefTime CefX509CertificateImpl::GetValidExpiry() {
  return valid_expiry_;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetDEREncoded() {
  return der_encoded_;
}

CefRefPtr<CefBinaryValue> CefX509CertificateImpl::GetPEMEncoded() {
  return pem_encoded_;
}

size_t CefX509CertificateImpl::GetIssuerChainSize() {
  return der_encoded_issuer_chain_.size();
}

void CefX509CertificateImpl::GetDEREncodedIssuerChain(
    CefX509Certificate::IssuerChainBinaryList& chain) {
  chain = der_encoded_issuer_chain_;
}

void CefX509CertificateImpl::GetPEMEncodedIssuerChain(
    CefX509Certificate::IssuerChainBinaryList& chain) {
  chain = pem_encoded_issuer_chain_;
}
