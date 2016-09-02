// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
#define CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
#pragma once

#include "include/cef_ssl_info.h"

#include "net/ssl/ssl_info.h"

// CefSSLInfo implementation
class CefSSLInfoImpl : public CefSSLInfo {
 public:
  explicit CefSSLInfoImpl(const net::SSLInfo& value);

  // CefSSLInfo methods.
  cef_cert_status_t GetCertStatus() override;
  CefRefPtr<CefX509Certificate> GetX509Certificate() override;

 private:
  cef_cert_status_t cert_status_;
  CefRefPtr<CefX509Certificate> cert_;

  IMPLEMENT_REFCOUNTING(CefSSLInfoImpl);
  DISALLOW_COPY_AND_ASSIGN(CefSSLInfoImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SSL_INFO_IMPL_H_
