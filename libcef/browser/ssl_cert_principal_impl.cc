// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/ssl_cert_principal_impl.h"

namespace {

void TransferVector(const std::vector<std::string>& source,
                    std::vector<CefString>& target) {
  if (!target.empty())
    target.clear();

  if (!source.empty()) {
    std::vector<std::string>::const_iterator it = source.begin();
    for (; it != source.end(); ++it)
      target.push_back(*it);
  }
}

}  // namespace

CefSSLCertPrincipalImpl::CefSSLCertPrincipalImpl(
    const net::CertPrincipal& value)
    : value_(value) {
}

CefString CefSSLCertPrincipalImpl::GetDisplayName() {
  return value_.GetDisplayName();
}

CefString CefSSLCertPrincipalImpl::GetCommonName() {
  return value_.common_name;
}

CefString CefSSLCertPrincipalImpl::GetLocalityName() {
  return value_.locality_name;
}

CefString CefSSLCertPrincipalImpl::GetStateOrProvinceName() {
  return value_.state_or_province_name;
}

CefString CefSSLCertPrincipalImpl::GetCountryName() {
  return value_.country_name;
}

void CefSSLCertPrincipalImpl::GetStreetAddresses(
    std::vector<CefString>& addresses) {
  TransferVector(value_.street_addresses, addresses);
}

void CefSSLCertPrincipalImpl::GetOrganizationNames(
    std::vector<CefString>& names) {
  TransferVector(value_.organization_names, names);
}

void CefSSLCertPrincipalImpl::GetOrganizationUnitNames(
    std::vector<CefString>& names) {
  TransferVector(value_.organization_unit_names, names);
}

void CefSSLCertPrincipalImpl::GetDomainComponents(
    std::vector<CefString>& components) {
  TransferVector(value_.domain_components, components);
}
