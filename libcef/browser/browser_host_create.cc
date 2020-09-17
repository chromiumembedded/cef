// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef_browser.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/chrome/chrome_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/features/runtime.h"

namespace {

class CreateBrowserHelper {
 public:
  CreateBrowserHelper(const CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient> client,
                      const CefString& url,
                      const CefBrowserSettings& settings,
                      CefRefPtr<CefDictionaryValue> extra_info,
                      CefRefPtr<CefRequestContext> request_context)
      : window_info_(windowInfo),
        client_(client),
        url_(url),
        settings_(settings),
        extra_info_(extra_info),
        request_context_(request_context) {}

  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefString url_;
  CefBrowserSettings settings_;
  CefRefPtr<CefDictionaryValue> extra_info_;
  CefRefPtr<CefRequestContext> request_context_;
};

}  // namespace

// static
bool CefBrowserHost::CreateBrowser(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  // Verify that the settings structure is a valid size.
  if (settings.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED() << "invalid CefBrowserSettings structure size";
    return false;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return false;
  }

  if (windowInfo.windowless_rendering_enabled &&
      !CefContext::Get()->settings().windowless_rendering_enabled) {
    LOG(ERROR) << "Creating a windowless browser without setting "
                  "CefSettings.windowless_rendering_enabled may result in "
                  "reduced performance or runtime errors.";
  }

  // Create the browser on the UI thread.
  CreateBrowserHelper* helper = new CreateBrowserHelper(
      windowInfo, client, url, settings, extra_info, request_context);
  CEF_POST_TASK(CEF_UIT, base::BindOnce(
                             [](CreateBrowserHelper* helper) {
                               CefBrowserHost::CreateBrowserSync(
                                   helper->window_info_, helper->client_,
                                   helper->url_, helper->settings_,
                                   helper->extra_info_,
                                   helper->request_context_);
                               delete helper;
                             },
                             helper));

  return true;
}

// static
CefRefPtr<CefBrowser> CefBrowserHost::CreateBrowserSync(
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return nullptr;
  }

  // Verify that the settings structure is a valid size.
  if (settings.size != sizeof(cef_browser_settings_t)) {
    NOTREACHED() << "invalid CefBrowserSettings structure size";
    return nullptr;
  }

  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return nullptr;
  }

  CefBrowserHostBase::CreateParams create_params;
  create_params.window_info.reset(new CefWindowInfo(windowInfo));
  create_params.client = client;
  create_params.url = GURL(url.ToString());
  if (!url.empty() && !create_params.url.is_valid() &&
      !create_params.url.has_scheme()) {
    std::string new_url = std::string("http://") + url.ToString();
    create_params.url = GURL(new_url);
  }
  create_params.settings = settings;
  create_params.extra_info = extra_info;
  create_params.request_context = request_context;

  if (cef::IsChromeRuntimeEnabled()) {
    auto browser = ChromeBrowserHostImpl::Create(create_params);
    return browser.get();
  }

  auto browser = CefBrowserHostImpl::Create(create_params);
  return browser.get();
}
