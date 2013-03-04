// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/scheme_registrar_impl.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_util.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"

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

  std::string scheme_lower = StringToLowerASCII(scheme_name.ToString());
  if (is_standard) {
    url_parse::Component scheme_comp(0, scheme_lower.length());
    if (!url_util::IsStandard(scheme_lower.c_str(), scheme_comp))
      url_util::AddStandardScheme(scheme_lower.c_str());
  }

  SchemeInfo info = {scheme_lower, is_local, is_display_isolated};
  scheme_info_list_.push_back(info);

  return true;
}

void CefSchemeRegistrarImpl::RegisterWithWebKit() {
  if (scheme_info_list_.empty())
    return;

  // Register the custom schemes.
  SchemeInfoList::const_iterator it = scheme_info_list_.begin();
  for (; it != scheme_info_list_.end(); ++it) {
    const SchemeInfo& info = *it;
    if (info.is_local) {
      WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(
          WebKit::WebString::fromUTF8(info.scheme_name));
    }
    if (info.is_display_isolated) {
      WebKit::WebSecurityPolicy::registerURLSchemeAsDisplayIsolated(
          WebKit::WebString::fromUTF8(info.scheme_name));
    }
  }
}

bool CefSchemeRegistrarImpl::VerifyRefCount() {
  return (GetRefCt() == 1);
}

void CefSchemeRegistrarImpl::Detach() {
  DCHECK_EQ(base::PlatformThread::CurrentId(), supported_thread_id_);
  url_util::LockStandardSchemes();
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
