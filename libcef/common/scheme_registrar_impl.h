// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
#define CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "include/cef_scheme.h"

#include "base/threading/platform_thread.h"
#include "content/public/common/content_client.h"

class CefSchemeRegistrarImpl : public CefSchemeRegistrar {
 public:
  CefSchemeRegistrarImpl();

  // CefSchemeRegistrar methods.
  bool AddCustomScheme(const CefString& scheme_name,
                       bool is_standard,
                       bool is_local,
                       bool is_display_isolated,
                       bool is_secure,
                       bool is_cors_enabled) override;

  void GetSchemes(content::ContentClient::Schemes* schemes);

  // Verify that only a single reference exists to all CefSchemeRegistrarImpl
  // objects.
  bool VerifyRefCount();

  void Detach();

 private:
  // Verify that the object is being accessed from the correct thread.
  bool VerifyContext();

  base::PlatformThreadId supported_thread_id_;

  content::ContentClient::Schemes schemes_;
  std::set<std::string> registered_schemes_;

  IMPLEMENT_REFCOUNTING(CefSchemeRegistrarImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSchemeRegistrarImpl);
};

#endif  // CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
