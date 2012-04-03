// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_POPUP_HANDLER_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_POPUP_HANDLER_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_handler.h"

// Handler for popup windows that loads the request in an existing browser
// window.
class ClientPopupHandler : public CefClient,
                           public CefRequestHandler {
 public:
  explicit ClientPopupHandler(CefRefPtr<CefBrowser> parentBrowser);
  virtual ~ClientPopupHandler();

  // CefClient methods
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }

  // CefRequestHandler methods
  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              NavType navType,
                              bool isRedirect) OVERRIDE;
 protected:
  CefRefPtr<CefBrowser> m_ParentBrowser;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientPopupHandler);
};

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_POPUP_HANDLER_H_
