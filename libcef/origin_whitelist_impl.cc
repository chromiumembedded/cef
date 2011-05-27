// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "cef_context.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

using WebKit::WebSecurityPolicy;
using WebKit::WebString;

bool CefAddCrossOriginWhitelistEntry(const CefString& source_origin,
                                     const CefString& target_protocol,
                                     const CefString& target_domain,
                                     bool allow_target_subdomains)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  std::string source_url = source_origin;
  GURL gurl = GURL(source_url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    NOTREACHED() << "Invalid source_origin URL: " << source_url;
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    std::string target_protocol_str = target_protocol;
    std::string target_domain_str = target_domain;
    WebSecurityPolicy::addOriginAccessWhitelistEntry(
        gurl,
        WebString::fromUTF8(target_protocol_str),
        WebString::fromUTF8(target_domain_str),
        allow_target_subdomains);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableFunction(&CefAddCrossOriginWhitelistEntry, source_origin,
                            target_protocol, target_domain,
                            allow_target_subdomains));
  }

  return true;
}

bool CefRemoveCrossOriginWhitelistEntry(const CefString& source_origin,
                                        const CefString& target_protocol,
                                        const CefString& target_domain,
                                        bool allow_target_subdomains)
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  std::string source_url = source_origin;
  GURL gurl = GURL(source_url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    NOTREACHED() << "Invalid source_origin URL: " << source_url;
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    std::string target_protocol_str = target_protocol;
    std::string target_domain_str = target_domain;
    WebSecurityPolicy::removeOriginAccessWhitelistEntry(
        gurl,
        WebString::fromUTF8(target_protocol_str),
        WebString::fromUTF8(target_domain_str),
        allow_target_subdomains);
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableFunction(&CefRemoveCrossOriginWhitelistEntry, source_origin,
                            target_protocol, target_domain,
                            allow_target_subdomains));
  }

  return true;
}

bool CefClearCrossOriginWhitelist()
{
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  if (CefThread::CurrentlyOn(CefThread::UI)) {
    WebSecurityPolicy::resetOriginAccessWhitelists();
  } else {
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        NewRunnableFunction(&CefClearCrossOriginWhitelist));
  }

  return true;
}
