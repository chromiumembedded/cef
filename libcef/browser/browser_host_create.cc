// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/include/cef_browser.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/chrome/views/chrome_child_window.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"

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
    DCHECK(false) << "context not valid";
    return false;
  }

  // Verify that the settings structure is a valid size.
  if (!CEF_MEMBER_EXISTS(&settings, chrome_zoom_bubble)) {
    DCHECK(false) << "invalid CefBrowserSettings structure size";
    return false;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      (!client || !client->GetRenderHandler().get())) {
    LOG(ERROR)
        << "Windowless rendering requires a CefRenderHandler implementation";
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
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  // Verify that the settings structure is a valid size.
  if (!CEF_MEMBER_EXISTS(&settings, chrome_zoom_bubble)) {
    DCHECK(false) << "invalid CefBrowserSettings structure size";
    return nullptr;
  }

  if (!request_context) {
    request_context = CefRequestContext::GetGlobalContext();
  }

  // Verify that the browser context is valid.
  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(request_context.get());
  if (!request_context_impl->VerifyBrowserContext()) {
    return nullptr;
  }

  // Verify windowless rendering requirements.
  if (windowInfo.windowless_rendering_enabled &&
      (!client || !client->GetRenderHandler().get())) {
    LOG(ERROR)
        << "Windowless rendering requires a CefRenderHandler implementation";
    return nullptr;
  }

  CefBrowserCreateParams create_params;
  create_params.MaybeSetWindowInfo(windowInfo, /*allow_alloy_style=*/true,
                                   /*allow_chrome_style=*/true);
  create_params.client = client;
  create_params.url = url;
  create_params.settings = settings;
  create_params.extra_info = extra_info;
  create_params.request_context = request_context;

  return CefBrowserHostBase::Create(create_params);
}

// static
CefRefPtr<CefBrowser> CefBrowserHost::GetBrowserByIdentifier(int browser_id) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  if (browser_id <= 0) {
    return nullptr;
  }

  return CefBrowserHostBase::GetBrowserForBrowserId(browser_id).get();
}

// static
bool CefBrowserCreateParams::IsChromeStyle(const CefWindowInfo* window_info) {
  if (!window_info) {
    return true;
  }

  // Both CHROME and DEFAULT indicate Chrome style with Chrome bootstrap.
  return window_info->runtime_style == CEF_RUNTIME_STYLE_CHROME ||
         window_info->runtime_style == CEF_RUNTIME_STYLE_DEFAULT;
}

bool CefBrowserCreateParams::IsChromeStyle() const {
  const bool chrome_style_via_window_info = IsChromeStyle(window_info.get());

  if (popup_with_alloy_style_opener) {
    // Creating a popup where the opener is Alloy style.
    if (chrome_style_via_window_info &&
        window_info->runtime_style == CEF_RUNTIME_STYLE_CHROME) {
      // Only use Chrome style for the popup if the client explicitly sets
      // CHROME (and not DEFAULT) via CefWindowInfo.runtime_style.
      return true;
    }
    return false;
  }

  if (browser_view) {
    // Must match the BrowserView style. GetRuntimeStyle() will not return
    // DEFAULT.
    return browser_view->GetRuntimeStyle() == CEF_RUNTIME_STYLE_CHROME;
  }

  // Chrome style does not support windowless rendering.
  return chrome_style_via_window_info && !IsWindowless();
}

bool CefBrowserCreateParams::IsWindowless() const {
  return window_info && window_info->windowless_rendering_enabled && client &&
         client->GetRenderHandler().get();
}

// static
void CefBrowserCreateParams::InitWindowInfo(CefWindowInfo* window_info,
                                            CefBrowserHostBase* opener) {
#if BUILDFLAG(IS_WIN)
  window_info->SetAsPopup(nullptr, CefString());
#endif

  if (opener->IsAlloyStyle()) {
    // Give the popup the same runtime style as the opener.
    window_info->runtime_style = CEF_RUNTIME_STYLE_ALLOY;
  }
}

void CefBrowserCreateParams::MaybeSetWindowInfo(
    const CefWindowInfo& new_window_info,
    bool allow_alloy_style,
    bool allow_chrome_style) {
  if (allow_chrome_style && new_window_info.windowless_rendering_enabled) {
    // Chrome style is not supported with windowles rendering.
    allow_chrome_style = false;
  }

#if BUILDFLAG(IS_MAC)
  if (allow_chrome_style &&
      chrome_child_window::HasParentHandle(new_window_info)) {
    // Chrome style is not supported with native parent on MacOS. See issue
    // #3294.
    allow_chrome_style = false;
  }
#endif

  DCHECK(allow_alloy_style || allow_chrome_style);

  bool reset_style = false;
  if (new_window_info.runtime_style == CEF_RUNTIME_STYLE_ALLOY &&
      !allow_alloy_style) {
    LOG(ERROR) << "Alloy style is not supported for this browser";
    reset_style = true;
  } else if (new_window_info.runtime_style == CEF_RUNTIME_STYLE_CHROME &&
             !allow_chrome_style) {
    LOG(ERROR) << "Chrome style is not supported for this browser";
    reset_style = true;
  }

  const bool is_chrome_style =
      allow_chrome_style && IsChromeStyle(&new_window_info);
  if (!is_chrome_style ||
      chrome_child_window::HasParentHandle(new_window_info)) {
    window_info = std::make_unique<CefWindowInfo>(new_window_info);
    if (!allow_chrome_style) {
      // Only Alloy style is allowed.
      window_info->runtime_style = CEF_RUNTIME_STYLE_ALLOY;
    } else if (reset_style) {
      // Use the default style.
      window_info->runtime_style = CEF_RUNTIME_STYLE_DEFAULT;
    }
  }
}

// static
CefRefPtr<CefBrowserHostBase> CefBrowserHostBase::Create(
    CefBrowserCreateParams& create_params) {
  if (create_params.IsChromeStyle()) {
    if (auto browser =
            chrome_child_window::MaybeCreateChildBrowser(create_params)) {
      return browser.get();
    }
    auto browser = ChromeBrowserHostImpl::Create(create_params);
    return browser.get();
  }

  auto browser = AlloyBrowserHostImpl::Create(create_params);
  return browser.get();
}
