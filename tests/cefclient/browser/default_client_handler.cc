// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/default_client_handler.h"

#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"

namespace client {

DefaultClientHandler::DefaultClientHandler(std::optional<bool> use_alloy_style)
    : use_alloy_style_(
          use_alloy_style.value_or(MainContext::Get()->UseAlloyStyleGlobal())) {
}

// static
CefRefPtr<DefaultClientHandler> DefaultClientHandler::GetForClient(
    CefRefPtr<CefClient> client) {
  auto base = BaseClientHandler::GetForClient(client);
  if (base && base->GetTypeKey() == &kTypeKey) {
    return static_cast<DefaultClientHandler*>(base.get());
  }
  return nullptr;
}

bool DefaultClientHandler::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int popup_id,
    const CefString& target_url,
    const CefString& target_frame_name,
    CefLifeSpanHandler::WindowOpenDisposition target_disposition,
    bool user_gesture,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info,
    bool* no_javascript_access) {
  CEF_REQUIRE_UI_THREAD();

  if (target_disposition == CEF_WOD_NEW_PICTURE_IN_PICTURE) {
    // Use default handling for document picture-in-picture popups.
    client = nullptr;
    return false;
  }

  // Used to configure default values.
  RootWindowConfig config(/*command_line=*/nullptr);

  // Potentially create a new RootWindow for the popup browser that will be
  // created asynchronously.
  MainContext::Get()->GetRootWindowManager()->CreateRootWindowAsPopup(
      config.use_views, use_alloy_style_, config.with_controls,
      /*is_osr=*/false, browser->GetIdentifier(), popup_id,
      /*is_devtools=*/false, popupFeatures, windowInfo, client, settings);

  // Allow popup creation.
  return false;
}

void DefaultClientHandler::OnBeforePopupAborted(CefRefPtr<CefBrowser> browser,
                                                int popup_id) {
  CEF_REQUIRE_UI_THREAD();
  MainContext::Get()->GetRootWindowManager()->AbortOrClosePopup(
      browser->GetIdentifier(), popup_id);
}

void DefaultClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Close all popups that have this browser as the opener.
  OnBeforePopupAborted(browser, /*popup_id=*/-1);

  BaseClientHandler::OnBeforeClose(browser);
}

}  // namespace client
