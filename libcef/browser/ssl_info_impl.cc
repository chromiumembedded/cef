// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_info_impl.h"
#include "libcef/browser/ssl_cert_principal_impl.h"
#include "libcef/common/time_util.h"

#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

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

CefSSLInfoImpl::CefSSLInfoImpl(const net::SSLInfo& value)
    : cert_status_(CERT_STATUS_NONE) {
  if (value.cert.get()) {
    cert_status_ = static_cast<cef_cert_status_t>(value.cert_status);

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
    if (os_handle)
      EncodeCertificate(os_handle, der_encoded_, pem_encoded_);

    const net::X509Certificate::OSCertHandles& issuer_chain =
      value.cert->GetIntermediateCertificates();
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
}

cef_cert_status_t CefSSLInfoImpl::GetCertStatus() {
   return cert_status_;
}

bool CefSSLInfoImpl::IsCertStatusError() {
   return net::IsCertStatusError(cert_status_);
}

bool CefSSLInfoImpl::IsCertStatusMinorError() {
   return net::IsCertStatusMinorError(cert_status_);
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

size_t CefSSLInfoImpl::GetIssuerChainSize() {
  return der_encoded_issuer_chain_.size();
}

void CefSSLInfoImpl::GetDEREncodedIssuerChain(
    CefSSLInfo::IssuerChainBinaryList& chain) {
  chain = der_encoded_issuer_chain_;
}

void CefSSLInfoImpl::GetPEMEncodedIssuerChain(
    CefSSLInfo::IssuerChainBinaryList& chain) {
  chain = pem_encoded_issuer_chain_;
}
