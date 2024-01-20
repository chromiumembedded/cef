// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_H_
#define CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_H_

#include "libcef/browser/browser_context.h"

#include "base/functional/callback_forward.h"
#include "net/cookies/canonical_cookie.h"

namespace net {
class HttpResponseHeaders;
}

namespace network {
struct ResourceRequest;
}  // namespace network

namespace net_service::cookie_helper {

// Returns true if the scheme for |url| supports cookies. |cookieable_schemes|
// is the optional list of schemes that the client has explicitly registered as
// cookieable, which may intentionally exclude standard schemes.
bool IsCookieableScheme(
    const GURL& url,
    const absl::optional<std::vector<std::string>>& cookieable_schemes);

using AllowCookieCallback =
    base::RepeatingCallback<void(const net::CanonicalCookie&,
                                 bool* /* allow */)>;
using DoneCookieCallback =
    base::OnceCallback<void(int /* total_count */,
                            net::CookieList /* allowed_cookies */)>;

// Load cookies for |request|. |allow_cookie_callback| will be executed for each
// cookie and should return true to allow it. |done_callback| will be executed
// on completion with |total_count| representing the total number of cookies
// retrieved, and |allowed_cookies| representing the list of cookies that were
// both retrieved and allowed by |allow_cookie_callback|. The loaded cookies
// will not be set on |request|; that should be done in |done_callback|. Must be
// called on the IO thread.
void LoadCookies(const CefBrowserContext::Getter& browser_context_getter,
                 const network::ResourceRequest& request,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback);

// Save cookies from |head|. |allow_cookie_callback| will be executed for each
// cookie and should return true to allow it. |done_callback| will be executed
// on completion with |total_count| representing the total number of cookies
// retrieved, and |allowed_cookies| representing the list of cookies that were
// both allowed by |allow_cookie_callback| an successfully saved. Must be called
// on the IO thread.
void SaveCookies(const CefBrowserContext::Getter& browser_context_getter,
                 const network::ResourceRequest& request,
                 net::HttpResponseHeaders* headers,
                 const AllowCookieCallback& allow_cookie_callback,
                 DoneCookieCallback done_callback);

}  // namespace net_service::cookie_helper

#endif  // CEF_LIBCEF_BROWSER_NET_SERVICE_COOKIE_HELPER_H_
