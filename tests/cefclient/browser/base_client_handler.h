// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_BASE_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_BASE_CLIENT_HANDLER_H_
#pragma once

#include "include/cef_client.h"
#include "include/wrapper/cef_message_router.h"
#include "tests/cefclient/browser/test_runner.h"

namespace client {

// Abstract base class for client handlers.
class BaseClientHandler : public CefClient,
                          public CefLifeSpanHandler,
                          public CefRequestHandler,
                          public CefResourceRequestHandler {
 public:
  BaseClientHandler();

  // Returns the BaseClientHandler associated with |browser|.
  static CefRefPtr<BaseClientHandler> GetForBrowser(
      CefRefPtr<CefBrowser> browser);

  // Returns the BaseClientHandler for |client|.
  static CefRefPtr<BaseClientHandler> GetForClient(CefRefPtr<CefClient> client);

  // CefClient methods
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  // CefLifeSpanHandler methods
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefRequestHandler methods
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override;
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    return this;
  }
  bool OnRenderProcessUnresponsive(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefUnresponsiveProcessCallback> callback) override;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override;

  // CefResourceRequestHandler methods
  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override;
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override;
  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override;

  // Returns the number of browsers currently using this handler. Can only be
  // called on the CEF UI thread.
  int GetBrowserCount() const;

  // Set a string resource for loading via StringResourceProvider.
  void SetStringResource(const std::string& page, const std::string& data);

  // Action to be taken when the render process becomes unresponsive.
  enum class HangAction {
    kDefault,
    kWait,
    kTerminate,
  };
  void SetHangAction(HangAction action);
  HangAction GetHangAction() const;

  // Used to determine the object type for each concrete implementation.
  virtual const void* GetTypeKey() const = 0;

 protected:
  CefRefPtr<CefResourceManager> GetResourceManager() const {
    return resource_manager_;
  }

 private:
  // The current number of browsers using this handler.
  int browser_count_ = 0;

  // Handles the browser side of query routing. The renderer side is handled
  // in client_renderer.cc.
  CefRefPtr<CefMessageRouterBrowserSide> message_router_;

  // Set of Handlers registered with the message router.
  test_runner::MessageHandlerSet message_handler_set_;

  // Manages the registration and delivery of resources.
  CefRefPtr<CefResourceManager> resource_manager_;

  // Used to manage string resources in combination with StringResourceProvider.
  // Only accessed on the IO thread.
  test_runner::StringResourceMap string_resource_map_;

  HangAction hang_action_ = HangAction::kDefault;

  DISALLOW_COPY_AND_ASSIGN(BaseClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_BASE_CLIENT_HANDLER_H_
