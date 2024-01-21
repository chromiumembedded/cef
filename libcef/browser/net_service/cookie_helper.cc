// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net_service/cookie_helper.h"

#include "libcef/browser/thread_util.h"
#include "libcef/common/net_service/net_service_util.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/cpp/resource_request.h"

namespace net_service::cookie_helper {

namespace {

// Do not keep a reference to the object returned by this method.
CefBrowserContext* GetBrowserContext(const CefBrowserContext::Getter& getter) {
  CEF_REQUIRE_UIT();
  DCHECK(!getter.is_null());

  // Will return nullptr if the BrowserContext has been shut down.
  return getter.Run();
}

// Do not keep a reference to the object returned by this method.
network::mojom::CookieManager* GetCookieManager(
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  return browser_context->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

net::CookieOptions GetCookieOptions(const network::ResourceRequest& request,
                                    bool for_loading_cookies) {
  // Match the logic from InterceptionJob::FetchCookies and
  // ChromeContentBrowserClient::ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel.
  bool should_treat_as_first_party =
      request.url.SchemeIsCryptographic() &&
      request.site_for_cookies.scheme() == content::kChromeUIScheme;
  bool is_main_frame_navigation =
      request.trusted_params &&
      request.trusted_params->isolation_info.request_type() ==
          net::IsolationInfo::RequestType::kMainFrame;

  // Match the logic from URLRequest::SetURLChain.
  std::vector<GURL> url_chain{request.url};
  if (request.navigation_redirect_chain.size() >= 2) {
    // Keep |request.url| as the final entry in the chain.
    url_chain.insert(url_chain.begin(),
                     request.navigation_redirect_chain.begin(),
                     request.navigation_redirect_chain.begin() +
                         request.navigation_redirect_chain.size() - 1);
  }

  net::CookieOptions options;
  options.set_include_httponly();
  if (for_loading_cookies) {
    // Match the logic from URLRequestHttpJob::AddCookieHeaderAndStart.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForRequest(
            request.method, url_chain, request.site_for_cookies,
            request.request_initiator, is_main_frame_navigation,
            should_treat_as_first_party));
  } else {
    // Match the logic from
    // URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete.
    options.set_same_site_cookie_context(
        net::cookie_util::ComputeSameSiteContextForResponse(
            url_chain, request.site_for_cookies, request.request_initiator,
            is_main_frame_navigation, should_treat_as_first_party));
  }

  return options;
}

//
// LOADING COOKIES.
//

void ContinueWithLoadedCookies(const AllowCookieCallback& allow_cookie_callback,
                               DoneCookieCallback done_callback,
                               const net::CookieAccessResultList& cookies) {
  CEF_REQUIRE_IOT();
  net::CookieList allowed_cookies;
  for (const auto& status : cookies) {
    bool allow = false;
    allow_cookie_callback.Run(status.cookie, &allow);
    if (allow) {
      allowed_cookies.push_back(status.cookie);
    }
  }
  std::move(done_callback).Run(cookies.size(), std::move(allowed_cookies));
}

void GetCookieListCallback(const AllowCookieCallback& allow_cookie_callback,
                           DoneCookieCallback done_callback,
                           const net::CookieAccessResultList& included_cookies,
                           const net::CookieAccessResultList&) {
  CEF_REQUIRE_UIT();
  CEF_POST_TASK(CEF_IOT,
                base::BindOnce(ContinueWithLoadedCookies, allow_cookie_callback,
                               std::move(done_callback), included_cookies));
}

void LoadCookiesOnUIThread(
    const CefBrowserContext::Getter& browser_context_getter,
    const GURL& url,
    const net::CookieOptions& options,
    net::CookiePartitionKeyCollection cookie_partition_key_collection,
    const AllowCookieCallback& allow_cookie_callback,
    DoneCookieCallback done_callback) {
  auto cef_browser_context = GetBrowserContext(browser_context_getter);
  auto browser_context =
      cef_browser_context ? cef_browser_context->AsBrowserContext() : nullptr;
  if (!browser_context) {
    GetCookieListCallback(allow_cookie_callback, std::move(done_callback),
                          net::CookieAccessResultList(),
                          net::CookieAccessResultList());
    return;
  }

  GetCookieManager(browser_context)
      ->GetCookieList(
          url, options, cookie_partition_key_collection,
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

void SetCanonicalCookieCallback(SaveCookiesProgress* progress,
                                const net::CanonicalCookie& cookie,
                                net::CookieAccessResult access_result) {
  CEF_REQUIRE_UIT();
  progress->num_cookie_lines_left_--;
  if (access_result.status.IsInclude()) {
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

void SaveCookiesOnUIThread(
    const CefBrowserContext::Getter& browser_context_getter,
    const GURL& url,
    const net::CookieOptions& options,
    int total_count,
    net::CookieList cookies,
    DoneCookieCallback done_callback) {
  DCHECK(!cookies.empty());

  auto cef_browser_context = GetBrowserContext(browser_context_getter);
  auto browser_context =
      cef_browser_context ? cef_browser_context->AsBrowserContext() : nullptr;
  if (!browser_context) {
    std::move(done_callback).Run(0, net::CookieList());
    return;
  }

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
        cookie, url, options,
        base::BindOnce(&SetCanonicalCookieCallback, base::Unretained(progress),
                       cookie));
  }

  SetCanonicalCookieCallback(
      progress, net::CanonicalCookie(),
      net::CookieAccessResult(net::CookieInclusionStatus(
          net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR)));
}

}  // namespace

bool IsCookieableScheme(
    const GURL& url,
    const absl::optional<std::vector<std::string>>& cookieable_schemes) {
  if (!url.has_scheme()) {
    return false;
  }

  if (cookieable_schemes) {
    // The client has explicitly registered the full set of schemes that should
    // be supported.
    const auto url_scheme = url.scheme_piece();
    for (auto scheme : *cookieable_schemes) {
      if (url_scheme == scheme) {
        return true;
      }
    }
    return false;
  }

  // Schemes that support cookies by default.
  // This should match CookieMonster::kDefaultCookieableSchemes.
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIsWSOrWSS();
}

void LoadCookies(const CefBrowserContext::Getter& browser_context_getter,
                 const network::ResourceRequest& request,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback) {
  CEF_REQUIRE_IOT();

  if ((request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES) ||
      request.credentials_mode == network::mojom::CredentialsMode::kOmit ||
      request.url.IsAboutBlank()) {
    // Continue immediately without loading cookies.
    std::move(done_callback).Run(0, {});
    return;
  }

  net::CookiePartitionKeyCollection partition_key_collection;
  if (request.trusted_params.has_value() &&
      !request.trusted_params->isolation_info.IsEmpty()) {
    partition_key_collection = net::CookiePartitionKeyCollection::FromOptional(
        net::CookiePartitionKey::FromNetworkIsolationKey(
            request.trusted_params->isolation_info.network_isolation_key()));
  }

  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(LoadCookiesOnUIThread, browser_context_getter, request.url,
                     GetCookieOptions(request, /*for_loading_cookies=*/true),
                     std::move(partition_key_collection), allow_cookie_callback,
                     std::move(done_callback)));
}

void SaveCookies(const CefBrowserContext::Getter& browser_context_getter,
                 const network::ResourceRequest& request,
                 net::HttpResponseHeaders* headers,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback) {
  CEF_REQUIRE_IOT();

  if (request.credentials_mode == network::mojom::CredentialsMode::kOmit ||
      request.url.IsAboutBlank() || !headers ||
      !headers->HasHeader(net_service::kHTTPSetCookieHeaderName)) {
    // Continue immediately without saving cookies.
    std::move(done_callback).Run(0, {});
    return;
  }

  // Match the logic in
  // URLRequestHttpJob::SaveCookiesAndNotifyHeadersComplete.
  base::Time response_date;
  if (!headers->GetDateValue(&response_date)) {
    response_date = base::Time();
  }

  const base::StringPiece name(net_service::kHTTPSetCookieHeaderName);
  std::string cookie_string;
  size_t iter = 0;
  net::CookieList allowed_cookies;
  int total_count = 0;

  while (headers->EnumerateHeader(&iter, name, &cookie_string)) {
    total_count++;

    net::CookieInclusionStatus returned_status;
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        request.url, cookie_string, base::Time::Now(),
        absl::make_optional(response_date),
        /*cookie_partition_key=*/absl::nullopt,
        /*block_truncated=*/true, &returned_status);
    if (!returned_status.IsInclude()) {
      continue;
    }

    bool allow = false;
    allow_cookie_callback.Run(*cookie, &allow);
    if (allow) {
      allowed_cookies.push_back(*cookie);
    }
  }

  if (!allowed_cookies.empty()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            SaveCookiesOnUIThread, browser_context_getter, request.url,
            GetCookieOptions(request, /*for_loading_cookies=*/false),
            total_count, std::move(allowed_cookies), std::move(done_callback)));

  } else {
    std::move(done_callback).Run(total_count, std::move(allowed_cookies));
  }
}

}  // namespace net_service::cookie_helper
