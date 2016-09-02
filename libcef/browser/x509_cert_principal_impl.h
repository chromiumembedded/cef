// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_X509_CERT_PRINCIPAL_IMPL_H_
#define CEF_LIBCEF_BROWSER_X509_CERT_PRINCIPAL_IMPL_H_
#pragma once

#include "include/cef_x509_certificate.h"

#include "net/cert/x509_cert_types.h"

// CefX509CertPrincipal implementation
class CefX509CertPrincipalImpl : public CefX509CertPrincipal {
 public:
  explicit CefX509CertPrincipalImpl(const net::CertPrincipal& value);

  // CefX509CertPrincipal methods.
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

  IMPLEMENT_REFCOUNTING(CefX509CertPrincipalImpl);
  DISALLOW_COPY_AND_ASSIGN(CefX509CertPrincipalImpl);
};

#endif  // CEF_LIBCEF_BROWSER_X509_CERT_PRINCIPAL_IMPL_H_
