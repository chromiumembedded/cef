// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#pragma once

#include <optional>

#include "tests/cefclient/browser/base_client_handler.h"

namespace client {

// Default client handler for unmanaged browser windows. Used with Chrome
// style only.
class DefaultClientHandler : public BaseClientHandler {
 public:
  // If |use_alloy_style| is nullopt the global default will be used.
  explicit DefaultClientHandler(
      std::optional<bool> use_alloy_style = std::nullopt);

  // Returns the DefaultClientHandler for |client|, or nullptr if |client| is
  // not a DefaultClientHandler.
  static CefRefPtr<DefaultClientHandler> GetForClient(
      CefRefPtr<CefClient> client);

 protected:
  bool OnBeforePopup(
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
      bool* no_javascript_access) override;
  void OnBeforePopupAborted(CefRefPtr<CefBrowser> browser,
                            int popup_id) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

 private:
  // Used to determine the object type.
  virtual const void* GetTypeKey() const override { return &kTypeKey; }
  static constexpr int kTypeKey = 0;

  const bool use_alloy_style_;

  IMPLEMENT_REFCOUNTING(DefaultClientHandler);
  DISALLOW_COPY_AND_ASSIGN(DefaultClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
