// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
#define CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "include/cef_scheme.h"

#include "content/public/common/content_client.h"

class CefSchemeRegistrarImpl : public CefSchemeRegistrar {
 public:
  CefSchemeRegistrarImpl();

  CefSchemeRegistrarImpl(const CefSchemeRegistrarImpl&) = delete;
  CefSchemeRegistrarImpl& operator=(const CefSchemeRegistrarImpl&) = delete;

  // CefSchemeRegistrar methods.
  bool AddCustomScheme(const CefString& scheme_name, int options) override;

  void GetSchemes(content::ContentClient::Schemes* schemes);

 private:
  content::ContentClient::Schemes schemes_;
  std::set<std::string> registered_schemes_;
};

#endif  // CEF_LIBCEF_COMMON_SCHEME_REGISTRAR_IMPL_H_
