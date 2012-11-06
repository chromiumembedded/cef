// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_network_delegate.h"

#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request.h"

BrowserNetworkDelegate::BrowserNetworkDelegate()
  : accept_all_cookies_(true) {
}

BrowserNetworkDelegate::~BrowserNetworkDelegate() {
}

int BrowserNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return net::OK;
}

int BrowserNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void BrowserNetworkDelegate::OnSendHeaders(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {
}

int BrowserNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>*override_response_headers) {
  return net::OK;
}

void BrowserNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                              const GURL& new_location) {
}

void BrowserNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
}

void BrowserNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                            int bytes_read) {
}
void BrowserNetworkDelegate::OnCompleted(net::URLRequest* request,
                                         bool started) {
}

void BrowserNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
}

void BrowserNetworkDelegate::OnPACScriptError(int line_number,
                              const string16& error) {
}

net::NetworkDelegate::AuthRequiredResponse
    BrowserNetworkDelegate::OnAuthRequired(
        net::URLRequest* request,
        const net::AuthChallengeInfo& auth_info,
        const AuthCallback& callback,
        net::AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool BrowserNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
  net::StaticCookiePolicy::Type policy_type = accept_all_cookies_ ?
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES :
      net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES;

  net::StaticCookiePolicy policy(policy_type);
  int rv = policy.CanGetCookies(
      request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool BrowserNetworkDelegate::OnCanSetCookie(
    const net::URLRequest& request,
    const std::string& cookie_line,
    net::CookieOptions* options) {
  net::StaticCookiePolicy::Type policy_type = accept_all_cookies_ ?
      net::StaticCookiePolicy::ALLOW_ALL_COOKIES :
      net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES;

  net::StaticCookiePolicy policy(policy_type);
  int rv = policy.CanSetCookie(
      request.url(), request.first_party_for_cookies());
  return rv == net::OK;
}

bool BrowserNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                             const FilePath& path) const {
  return true;
}

bool BrowserNetworkDelegate::OnCanThrottleRequest(
    const net::URLRequest& request) const {
  return false;
}

int BrowserNetworkDelegate::OnBeforeSocketStreamConnect(
    net::SocketStream* stream,
    const net::CompletionCallback& callback) {
  return net::OK;
}

void BrowserNetworkDelegate::OnRequestWaitStateChange(
    const net::URLRequest& request,
    RequestWaitState state) {
}
