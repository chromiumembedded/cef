// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_RESOURCE_REQUEST_JOB_H_
#define CEF_LIBCEF_BROWSER_RESOURCE_REQUEST_JOB_H_

#include <stdint.h>

#include <string>

#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_request_handler.h"

#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_job.h"

namespace net {
class HttpResponseHeaders;
class URLRequest;
}

class CefResourceRequestJobCallback;

class CefResourceRequestJob : public net::URLRequestJob {
 public:
  CefResourceRequestJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        CefRefPtr<CefResourceHandler> handler);
  ~CefResourceRequestJob() override;

 private:
  // net::URLRequestJob methods.
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* dest, int dest_size) override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  void GetLoadTimingInfo(
      net::LoadTimingInfo* load_timing_info) const override;
  bool IsRedirectResponse(GURL* location, int* http_status_code)
      override;
  bool GetMimeType(std::string* mime_type) const override;

  void SendHeaders();

  // Used for sending cookies with the request.
  void AddCookieHeaderAndStart();
  void DoLoadCookies();
  void CheckCookiePolicyAndLoad(const net::CookieList& cookie_list);
  void OnCookiesLoaded(const std::string& cookie_line);
  void DoStartTransaction();
  void StartTransaction();

  // Used for saving cookies returned as part of the response.
  net::HttpResponseHeaders* GetResponseHeaders();
  void SaveCookiesAndNotifyHeadersComplete();
  void SaveNextCookie();
  void OnCookieSaved(bool cookie_status);
  void CookieHandled();
  void FetchResponseCookies(std::vector<std::string>* cookies);

  void DoneWithRequest();

  CefRefPtr<CefResourceHandler> handler_;
  bool done_;
  CefRefPtr<CefResponse> response_;
  GURL redirect_url_;
  int64_t remaining_bytes_;
  int64_t sent_bytes_;
  CefRefPtr<CefRequest> cef_request_;
  CefRefPtr<CefResourceRequestJobCallback> callback_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  std::vector<std::string> response_cookies_;
  size_t response_cookies_save_index_;
  base::Time request_start_time_;
  base::TimeTicks receive_headers_end_;

  // Must be the last member.
  base::WeakPtrFactory<CefResourceRequestJob> weak_factory_;

  friend class CefResourceRequestJobCallback;

  DISALLOW_COPY_AND_ASSIGN(CefResourceRequestJob);
};

#endif  // CEF_LIBCEF_BROWSER_RESOURCE_REQUEST_JOB_H_
