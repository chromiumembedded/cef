// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SSL_CERT_PRINCIPAL_IMPL_H_
#define CEF_LIBCEF_BROWSER_SSL_CERT_PRINCIPAL_IMPL_H_
#pragma once

#include "include/cef_ssl_info.h"

#include "net/cert/x509_cert_types.h"

// CefSSLCertPrincipal implementation
class CefSSLCertPrincipalImpl : public CefSSLCertPrincipal {
 public:
  explicit CefSSLCertPrincipalImpl(const net::CertPrincipal& value);

  // CefSSLCertPrincipal methods.
  CefString GetDisplayName() override;
  CefString GetCommonName() override;
  CefString GetLocalityName() override;
  CefString GetStateOrProvinceName() override;
  CefString GetCountryName() override;
  void GetStreetAddresses(std::vector<CefString>& addresses) override;
  void GetOrganizationNames(std::vector<CefString>& names) override;
  void GetOrganizationUnitNames(std::vector<CefString>& names) override;
  void GetDomainComponents(std::vector<CefString>& components) override;

 private:
  net::CertPrincipal value_;

  IMPLEMENT_REFCOUNTING(CefSSLCertPrincipalImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSSLCertPrincipalImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SSL_CERT_PRINCIPAL_IMPL_H_
