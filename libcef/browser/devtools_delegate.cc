// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/devtools_delegate.h"

#include <algorithm>

#include "content/public/browser/devtools_http_handler.h"
#include "content/public/common/content_client.h"
#include "grit/cef_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

CefDevToolsDelegate::CefDevToolsDelegate(
    int port,
    net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter) {
  devtools_http_handler_ = content::DevToolsHttpHandler::Start(
      "127.0.0.1",
      port,
      "",
      this);
}

CefDevToolsDelegate::~CefDevToolsDelegate() {
}

void CefDevToolsDelegate::Stop() {
  // The call below destroys this.
  devtools_http_handler_->Stop();
}

std::string CefDevToolsDelegate::GetDiscoveryPageHTML() {
  return content::GetContentClient()->GetDataResource(
      IDR_CEF_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

net::URLRequestContext*
CefDevToolsDelegate::GetURLRequestContext() {
  return context_getter_->GetURLRequestContext();
}

bool CefDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

std::string CefDevToolsDelegate::GetFrontendResourcesBaseURL() {
  return "";
}
