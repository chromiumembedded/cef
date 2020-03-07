// Copyright (c) 2019 The Chromium Embedded Framework Authors. Portions
// Copyright (c) 2018 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "libcef/common/net_service/net_service_util.h"

#include "libcef/common/time_util.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"

namespace net_service {

namespace {

// Determine the cookie domain to use for setting the specified cookie.
// From net/cookies/cookie_store.cc.
bool GetCookieDomain(const GURL& url,
                     const net::ParsedCookie& pc,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return net::cookie_util::GetCookieDomainWithString(url, domain_string,
                                                     result);
}

}  // namespace

const char kHTTPLocationHeaderName[] = "Location";
const char kHTTPSetCookieHeaderName[] = "Set-Cookie";

const char kContentTypeApplicationFormURLEncoded[] =
    "application/x-www-form-urlencoded";

const char kHTTPHeaderSep[] = ": ";

std::string MakeHeader(const std::string& name, const std::string& value) {
  std::string header(name);
  header.append(kHTTPHeaderSep);
  header.append(value);
  return header;
}

std::string MakeStatusLine(int status_code,
                           const std::string& status_text,
                           bool for_replacement) {
  std::string status("HTTP/1.1 ");
  status.append(base::NumberToString(status_code));
  status.append(" ");

  if (status_text.empty()) {
    const std::string& text =
        net::GetHttpReasonPhrase(static_cast<net::HttpStatusCode>(status_code));
    DCHECK(!text.empty());
    status.append(text);
  } else {
    status.append(status_text);
  }

  if (!for_replacement) {
    // The HttpResponseHeaders constructor expects its input string to be
    // terminated by two NULs.
    status.append("\0\0", 2);
  }
  return status;
}

std::string MakeContentTypeValue(const std::string& mime_type,
                                 const std::string& charset) {
  DCHECK(!mime_type.empty());
  std::string value = mime_type;
  if (!charset.empty()) {
    value.append("; charset=");
    value.append(charset);
  }
  return value;
}

scoped_refptr<net::HttpResponseHeaders> MakeResponseHeaders(
    int status_code,
    const std::string& status_text,
    const std::string& mime_type,
    const std::string& charset,
    int64_t content_length,
    const std::multimap<std::string, std::string>& extra_headers,
    bool allow_existing_header_override) {
  if (status_code <= 0)
    status_code = 200;

  auto headers = WrapRefCounted(new net::HttpResponseHeaders(
      MakeStatusLine(status_code, status_text, false)));

  // Track the headers that have already been set. Perform all comparisons in
  // lowercase.
  std::set<std::string> set_headers_lowercase;

  if (status_code == net::HTTP_OK) {
    if (!mime_type.empty()) {
      headers->AddHeader(MakeHeader(net::HttpRequestHeaders::kContentType,
                                    MakeContentTypeValue(mime_type, charset)));
      set_headers_lowercase.insert(
          base::ToLowerASCII(net::HttpRequestHeaders::kContentType));
    }

    if (content_length >= 0) {
      headers->AddHeader(MakeHeader(net::HttpRequestHeaders::kContentLength,
                                    base::NumberToString(content_length)));
      set_headers_lowercase.insert(
          base::ToLowerASCII(net::HttpRequestHeaders::kContentLength));
    }
  }

  for (const auto& pair : extra_headers) {
    if (!set_headers_lowercase.empty()) {
      // Check if the header has already been set.
      const std::string& name_lowercase = base::ToLowerASCII(pair.first);
      if (set_headers_lowercase.find(name_lowercase) !=
          set_headers_lowercase.end()) {
        if (allow_existing_header_override)
          headers->RemoveHeader(pair.first);
        else
          continue;
      }
    }

    headers->AddHeader(MakeHeader(pair.first, pair.second));
  }

  return headers;
}

net::RedirectInfo MakeRedirectInfo(const network::ResourceRequest& request,
                                   const net::HttpResponseHeaders* headers,
                                   const GURL& new_location,
                                   int status_code) {
  bool insecure_scheme_was_upgraded = false;

  GURL location = new_location;
  if (status_code == 0)
    status_code = net::HTTP_TEMPORARY_REDIRECT;

  // If this a redirect to HTTP of a request that had the
  // 'upgrade-insecure-requests' policy set, upgrade it to HTTPS.
  if (request.upgrade_if_insecure) {
    if (location.SchemeIs("http")) {
      insecure_scheme_was_upgraded = true;
      GURL::Replacements replacements;
      replacements.SetSchemeStr("https");
      location = location.ReplaceComponents(replacements);
    }
  }

  net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
      request.update_first_party_url_on_redirect
          ? net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      request.method, request.url, request.site_for_cookies,
      first_party_url_policy, request.referrer_policy, request.referrer.spec(),
      status_code, location,
      net::RedirectUtil::GetReferrerPolicyHeader(headers),
      insecure_scheme_was_upgraded);
}

bool MakeCefCookie(const net::CanonicalCookie& cc, CefCookie& cookie) {
  CefString(&cookie.name).FromString(cc.Name());
  CefString(&cookie.value).FromString(cc.Value());
  CefString(&cookie.domain).FromString(cc.Domain());
  CefString(&cookie.path).FromString(cc.Path());
  cookie.secure = cc.IsSecure();
  cookie.httponly = cc.IsHttpOnly();
  cef_time_from_basetime(cc.CreationDate(), cookie.creation);
  cef_time_from_basetime(cc.LastAccessDate(), cookie.last_access);
  cookie.has_expires = cc.IsPersistent();
  if (cookie.has_expires)
    cef_time_from_basetime(cc.ExpiryDate(), cookie.expires);

  return true;
}

bool MakeCefCookie(const GURL& url,
                   const std::string& cookie_line,
                   CefCookie& cookie) {
  // Parse the cookie.
  net::ParsedCookie pc(cookie_line);
  if (!pc.IsValid())
    return false;

  std::string cookie_domain;
  if (!GetCookieDomain(url, pc, &cookie_domain))
    return false;

  std::string path_string;
  if (pc.HasPath())
    path_string = pc.Path();
  std::string cookie_path =
      net::CanonicalCookie::CanonPathWithString(url, path_string);
  base::Time creation_time = base::Time::Now();
  base::Time cookie_expires =
      net::CanonicalCookie::CanonExpiration(pc, creation_time, creation_time);

  CefString(&cookie.name).FromString(pc.Name());
  CefString(&cookie.value).FromString(pc.Value());
  CefString(&cookie.domain).FromString(cookie_domain);
  CefString(&cookie.path).FromString(cookie_path);
  cookie.secure = pc.IsSecure();
  cookie.httponly = pc.IsHttpOnly();
  cef_time_from_basetime(creation_time, cookie.creation);
  cef_time_from_basetime(creation_time, cookie.last_access);
  cookie.has_expires = !cookie_expires.is_null();
  if (cookie.has_expires)
    cef_time_from_basetime(cookie_expires, cookie.expires);

  return true;
}

}  // namespace net_service