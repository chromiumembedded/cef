// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_helper.h"

#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace net_service {

namespace {

// Do not keep a reference to the CookieManager returned by this method.
network::mojom::CookieManager* GetCookieManager(
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  return content::BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetCookieManagerForBrowserProcess();
}

//
// LOADING COOKIES.
//

void ContinueWithLoadedCookies(const AllowCookieCallback& allow_cookie_callback,
                               DoneCookieCallback done_callback,
                               const net::CookieList& cookies) {
  CEF_REQUIRE_IOT();
  net::CookieList allowed_cookies;
  for (const auto& cookie : cookies) {
    bool allow = false;
    allow_cookie_callback.Run(cookie, &allow);
    if (allow)
      allowed_cookies.push_back(cookie);
  }
  std::move(done_callback).Run(cookies.size(), std::move(allowed_cookies));
}

void GetCookieListCallback(const AllowCookieCallback& allow_cookie_callback,
                           DoneCookieCallback done_callback,
                           const net::CookieList& cookies,
                           const net::CookieStatusList&) {
  CEF_REQUIRE_UIT();
  CEF_POST_TASK(CEF_IOT,
                base::BindOnce(ContinueWithLoadedCookies, allow_cookie_callback,
                               std::move(done_callback), cookies));
}

void LoadCookiesOnUIThread(content::BrowserContext* browser_context,
                           const GURL& url,
                           const net::CookieOptions& options,
                           const AllowCookieCallback& allow_cookie_callback,
                           DoneCookieCallback done_callback) {
  CEF_REQUIRE_UIT();
  GetCookieManager(browser_context)
      ->GetCookieList(
          url, options,
          base::BindOnce(GetCookieListCallback, allow_cookie_callback,
                         std::move(done_callback)));
}

//
// SAVING COOKIES.
//

struct SaveCookiesProgress {
  DoneCookieCallback done_callback_;
  int total_count_;
  net::CookieList allowed_cookies_;
  int num_cookie_lines_left_;
};

void SetCanonicalCookieCallback(
    SaveCookiesProgress* progress,
    const net::CanonicalCookie& cookie,
    net::CanonicalCookie::CookieInclusionStatus status) {
  CEF_REQUIRE_UIT();
  progress->num_cookie_lines_left_--;
  if (status == net::CanonicalCookie::CookieInclusionStatus::INCLUDE) {
    progress->allowed_cookies_.push_back(cookie);
  }

  // If all the cookie lines have been handled the request can be continued.
  if (progress->num_cookie_lines_left_ == 0) {
    CEF_POST_TASK(CEF_IOT,
                  base::BindOnce(std::move(progress->done_callback_),
                                 progress->total_count_,
                                 std::move(progress->allowed_cookies_)));
    delete progress;
  }
}

void SaveCookiesOnUIThread(content::BrowserContext* browser_context,
                           const GURL& url,
                           const net::CookieOptions& options,
                           int total_count,
                           net::CookieList cookies,
                           DoneCookieCallback done_callback) {
  CEF_REQUIRE_UIT();
  DCHECK(!cookies.empty());

  network::mojom::CookieManager* cookie_manager =
      GetCookieManager(browser_context);

  // |done_callback| needs to be executed once and only once after the list has
  // been fully processed. |num_cookie_lines_left_| keeps track of how many
  // async callbacks are currently pending.
  auto progress = new SaveCookiesProgress;
  progress->done_callback_ = std::move(done_callback);
  progress->total_count_ = total_count;

  // Make sure to wait for the loop to complete.
  progress->num_cookie_lines_left_ = 1;

  for (const auto& cookie : cookies) {
    progress->num_cookie_lines_left_++;
    cookie_manager->SetCanonicalCookie(
        cookie, url.scheme(), options,
        base::BindOnce(&SetCanonicalCookieCallback, base::Unretained(progress),
                       cookie));
  }

  SetCanonicalCookieCallback(
      progress, net::CanonicalCookie(),
      net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
}

}  // namespace

void LoadCookies(content::BrowserContext* browser_context,
                 const network::ResourceRequest& request,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback) {
  CEF_REQUIRE_IOT();

  if ((request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES) ||
      !request.allow_credentials) {
    // Continue immediately without loading cookies.
    std::move(done_callback).Run(0, {});
    return;
  }

  // Match the logic in URLRequestHttpJob::AddCookieHeaderAndStart.
  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForRequest(
          request.method, request.url, request.site_for_cookies,
          request.request_initiator, request.attach_same_site_cookies));

  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(LoadCookiesOnUIThread, browser_context, request.url,
                     options, allow_cookie_callback, std::move(done_callback)));
}

void SaveCookies(content::BrowserContext* browser_context,
                 const network::ResourceRequest& request,
                 const network::ResourceResponseHead& head,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback) {
  CEF_REQUIRE_IOT();

  if (!request.allow_credentials ||
      !head.headers->HasHeader(net_service::kHTTPSetCookieHeaderName)) {
    // Continue immediately without saving cookies.
    std::move(done_callback).Run(0, {});
    return;
  }

  // Match the logic in
  // URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete.
  base::Time response_date;
  if (!head.headers->GetDateValue(&response_date))
    response_date = base::Time();

  net::CookieOptions options;
  options.set_include_httponly();
  options.set_server_time(response_date);
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForRequest(
          request.method, request.url, request.site_for_cookies,
          request.request_initiator, request.attach_same_site_cookies));

  const base::StringPiece name(net_service::kHTTPSetCookieHeaderName);
  std::string cookie_string;
  size_t iter = 0;
  net::CookieList allowed_cookies;
  int total_count = 0;

  while (head.headers->EnumerateHeader(&iter, name, &cookie_string)) {
    total_count++;

    net::CanonicalCookie::CookieInclusionStatus returned_status;
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        request.url, cookie_string, base::Time::Now(), options,
        &returned_status);
    if (returned_status !=
        net::CanonicalCookie::CookieInclusionStatus::INCLUDE) {
      continue;
    }

    bool allow = false;
    allow_cookie_callback.Run(*cookie, &allow);
    if (allow)
      allowed_cookies.push_back(*cookie);
  }

  if (!allowed_cookies.empty()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(SaveCookiesOnUIThread, browser_context, request.url,
                       options, total_count, std::move(allowed_cookies),
                       std::move(done_callback)));

  } else {
    std::move(done_callback).Run(total_count, std::move(allowed_cookies));
  }
}

}  // namespace net_service