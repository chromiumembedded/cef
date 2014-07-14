// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/scheme_registrar_impl.h"

#include <string>

#include "libcef/common/content_client.h"

#include "base/bind.h"
#include "base/logging.h"

CefSchemeRegistrarImpl::CefSchemeRegistrarImpl()
    : supported_thread_id_(base::PlatformThread::CurrentId()) {
}

bool CefSchemeRegistrarImpl::AddCustomScheme(
    const CefString& scheme_name,
    bool is_standard,
    bool is_local,
    bool is_display_isolated) {
  if (!VerifyContext())
    return false;

  const std::string& scheme = scheme_name;

  if (is_standard)
    standard_schemes_.push_back(scheme);

  CefContentClient::SchemeInfo scheme_info = {
      scheme, is_standard, is_local, is_display_isolated};
  CefContentClient::Get()->AddCustomScheme(scheme_info);

  return true;
}

void CefSchemeRegistrarImpl::GetStandardSchemes(
    std::vector<std::string>* standard_schemes) {
  if (!VerifyContext())
    return;

  if (standard_schemes_.empty())
    return;

  standard_schemes->insert(standard_schemes->end(), standard_schemes_.begin(),
                           standard_schemes_.end());
}

bool CefSchemeRegistrarImpl::VerifyRefCount() {
  return HasOneRef();
}

void CefSchemeRegistrarImpl::Detach() {
  if (VerifyContext())
    supported_thread_id_ = base::kInvalidThreadId;
}

bool CefSchemeRegistrarImpl::VerifyContext() {
  if (base::PlatformThread::CurrentId() != supported_thread_id_) {
    // This object should only be accessed from the thread that created it.
    NOTREACHED();
    return false;
  }

  return true;
}
