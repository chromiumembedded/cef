// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
#pragma once

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

// Used for intercepting resource requests, redirects and responses. The single
// instance of this class is managed by CefURLRequestContextGetter.
class CefNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  CefNetworkDelegate();
  ~CefNetworkDelegate() override;

  // Match the logic from ChromeNetworkDelegate and
  // RenderFrameMessageFilter::OnSetCookie.
  static bool AreExperimentalCookieFeaturesEnabled();
  static bool AreStrictSecureCookiesEnabled();

 private:
  // net::NetworkDelegate methods.
  int OnBeforeURLRequest(net::URLRequest* request,
                         const net::CompletionCallback& callback,
                         GURL* new_url) override;
  AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      const AuthCallback& callback,
      net::AuthCredentials* credentials) override;
  void OnCompleted(net::URLRequest* request, bool started) override;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& path) const override;
  bool OnAreExperimentalCookieFeaturesEnabled() const override;
  bool OnAreStrictSecureCookiesEnabled() const override;
  net::Filter* SetupFilter(net::URLRequest* request,
                           net::Filter* filter_list) override;

  DISALLOW_COPY_AND_ASSIGN(CefNetworkDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
