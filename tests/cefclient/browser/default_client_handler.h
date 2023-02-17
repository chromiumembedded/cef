// Copyright (c) 2023 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
#pragma once

#include "include/cef_client.h"
#include "include/wrapper/cef_resource_manager.h"

namespace client {

// Default client handler for unmanaged browser windows. Used with the Chrome
// runtime only.
class DefaultClientHandler : public CefClient,
                             public CefRequestHandler,
                             public CefResourceRequestHandler {
 public:
  DefaultClientHandler();

  // CefClient methods
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

  // CefRequestHandler methods
  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override;

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

 private:
  // Manages the registration and delivery of resources.
  CefRefPtr<CefResourceManager> resource_manager_;

  IMPLEMENT_REFCOUNTING(DefaultClientHandler);
  DISALLOW_COPY_AND_ASSIGN(DefaultClientHandler);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_DEFAULT_CLIENT_HANDLER_H_
