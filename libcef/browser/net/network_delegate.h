// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
#pragma once

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

template <class T>
class PrefMember;
typedef PrefMember<bool> BooleanPrefMember;

// Used for intercepting resource requests, redirects and responses. The single
// instance of this class is managed by CefURLRequestContextGetter.
class CefNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  CefNetworkDelegate();
  ~CefNetworkDelegate() override;

  void set_force_google_safesearch(BooleanPrefMember* force_google_safesearch) {
    force_google_safesearch_ = force_google_safesearch;
  }

 private:
  // net::NetworkDelegate methods.
  std::unique_ptr<net::SourceStream> CreateSourceStream(
      net::URLRequest* request,
      std::unique_ptr<net::SourceStream> upstream) override;
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  AuthRequiredResponse OnAuthRequired(
      net::URLRequest* request,
      const net::AuthChallengeInfo& auth_info,
      AuthCallback callback,
      net::AuthCredentials* credentials) override;
  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options,
                      bool allowed_from_caller) override;
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  // Weak, owned by our owner (CefURLRequestContextGetter).
  BooleanPrefMember* force_google_safesearch_;

  DISALLOW_COPY_AND_ASSIGN(CefNetworkDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_NET_NETWORK_DELEGATE_H_
