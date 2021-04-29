// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cef_browser.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
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

  void Run() {
    CefBrowserHost::CreateBrowserSync(window_info_, client_, url_, settings_,
                                      extra_info_, request_context_);
  }

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

  if (!request_context) {
    request_context = CefRequestContext::GetGlobalContext();
  }

  auto helper = std::make_unique<CreateBrowserHelper>(
      windowInfo, client, url, settings, extra_info, request_context);

  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(request_context.get());

  // Wait for the browser context to be initialized before creating the browser.
  request_context_impl->ExecuteWhenBrowserContextInitialized(base::BindOnce(
      [](std::unique_ptr<CreateBrowserHelper> helper) {
        // Always execute asynchronously to avoid potential issues if we're
        // being called synchronously during app initialization.
        CEF_POST_TASK(CEF_UIT, base::BindOnce(&CreateBrowserHelper::Run,
                                              std::move(helper)));
      },
      std::move(helper)));

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

  // Verify that the browser context is valid.
  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(request_context.get());
  if (!request_context_impl->VerifyBrowserContext()) {
    return nullptr;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      !client->GetRenderHandler().get()) {
    NOTREACHED() << "CefRenderHandler implementation is required";
    return nullptr;
  }

  CefBrowserCreateParams create_params;
  create_params.window_info.reset(new CefWindowInfo(windowInfo));
  create_params.client = client;
  create_params.url = url;
  create_params.settings = settings;
  create_params.extra_info = extra_info;
  create_params.request_context = request_context;

  return CefBrowserHostBase::Create(create_params);
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::Create(
    CefBrowserCreateParams& create_params) {
  if (cef::IsChromeRuntimeEnabled()) {
    auto browser = ChromeBrowserHostImpl::Create(create_params);
    return browser.get();
  }

  auto browser = AlloyBrowserHostImpl::Create(create_params);
  return browser.get();
}
