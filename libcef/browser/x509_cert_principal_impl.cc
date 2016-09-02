// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/x509_cert_principal_impl.h"

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

CefX509CertPrincipalImpl::CefX509CertPrincipalImpl(
    const net::CertPrincipal& value)
    : value_(value) {
}

CefString CefX509CertPrincipalImpl::GetDisplayName() {
  return value_.GetDisplayName();
}

CefString CefX509CertPrincipalImpl::GetCommonName() {
  return value_.common_name;
}

CefString CefX509CertPrincipalImpl::GetLocalityName() {
  return value_.locality_name;
}

CefString CefX509CertPrincipalImpl::GetStateOrProvinceName() {
  return value_.state_or_province_name;
}

CefString CefX509CertPrincipalImpl::GetCountryName() {
  return value_.country_name;
}

void CefX509CertPrincipalImpl::GetStreetAddresses(
    std::vector<CefString>& addresses) {
  TransferVector(value_.street_addresses, addresses);
}

void CefX509CertPrincipalImpl::GetOrganizationNames(
    std::vector<CefString>& names) {
  TransferVector(value_.organization_names, names);
}

void CefX509CertPrincipalImpl::GetOrganizationUnitNames(
    std::vector<CefString>& names) {
  TransferVector(value_.organization_unit_names, names);
}

void CefX509CertPrincipalImpl::GetDomainComponents(
    std::vector<CefString>& components) {
  TransferVector(value_.domain_components, components);
}
