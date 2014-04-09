// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_URL_NETWORK_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_URL_NETWORK_DELEGATE_H_
#pragma once

#include "net/base/network_delegate.h"

// Used for intercepting resource requests, redirects and responses. The single
// instance of this class is managed by CefURLRequestContextGetter.
class CefNetworkDelegate : public net::NetworkDelegate {
 public:
  CefNetworkDelegate();
  ~CefNetworkDelegate();

 private:
  // net::NetworkDelegate methods.
  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const net::CompletionCallback& callback,
                                 GURL* new_url) OVERRIDE;
  virtual AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) OVERRIDE;
  virtual bool OnCanAccessFile(const net::URLRequest& request,
                               const base::FilePath& path) const;

  DISALLOW_COPY_AND_ASSIGN(CefNetworkDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_URL_NETWORK_DELEGATE_H_
