// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CLIENT_POPUP_HANDLER_H
#define _CLIENT_POPUP_HANDLER_H

#include "include/cef.h"

// Handler for popup windows that loads the request in an existing browser
// window.
class ClientPopupHandler : public CefClient,
                           public CefRequestHandler
{
public:
  ClientPopupHandler(CefRefPtr<CefBrowser> parentBrowser);
  virtual ~ClientPopupHandler();

  // CefClient methods
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE
      { return this; }

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

#endif // _CLIENT_POPUP_HANDLER_H
