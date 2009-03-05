// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _HANDLER_CTOCPP_H
#define _HANDLER_CTOCPP_H

#ifndef BUILDING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed DLL-side only")
#else // BUILDING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C handler structure with a C++ handler class.
// This class may be instantiated and accessed DLL-side only.
class CefHandlerCToCpp : public CefCToCpp<CefHandler, cef_handler_t>
{
public:
  CefHandlerCToCpp(cef_handler_t* str)
    : CefCToCpp<CefHandler, cef_handler_t>(str) {}
  virtual ~CefHandlerCToCpp() {}

  // CefHandler methods
  virtual RetVal HandleBeforeCreated(CefRefPtr<CefBrowser> parentBrowser,
                                     CefWindowInfo& windowInfo, bool popup,
                                     CefRefPtr<CefHandler>& handler,
                                     std::wstring& url);
  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser);
  virtual RetVal HandleAddressChange(CefRefPtr<CefBrowser> browser,
                                     const std::wstring& url);
  virtual RetVal HandleTitleChange(CefRefPtr<CefBrowser> browser,
                                   const std::wstring& title);
  virtual RetVal HandleBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect);
  virtual RetVal HandleLoadStart(CefRefPtr<CefBrowser> browser);
  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser);
  virtual RetVal HandleLoadError(CefRefPtr<CefBrowser> browser,
                                 ErrorCode errorCode,
                                 const std::wstring& failedUrl,
                                 std::wstring& errorText);
  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          std::wstring& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          std::wstring& mimeType,
                                          int loadFlags);
  virtual RetVal HandleBeforeMenu(CefRefPtr<CefBrowser> browser,
                                  const MenuInfo& menuInfo);
  virtual RetVal HandleGetMenuLabel(CefRefPtr<CefBrowser> browser,
                                    MenuId menuId, std::wstring& label);
  virtual RetVal HandleMenuAction(CefRefPtr<CefBrowser> browser,
                                  MenuId menuId);
  virtual RetVal HandlePrintHeaderFooter(CefRefPtr<CefBrowser> browser,
                                         CefPrintInfo& printInfo,
                                         const std::wstring& url,
                                         const std::wstring& title,
                                         int currentPage, int maxPages,
                                         std::wstring& topLeft,
                                         std::wstring& topCenter,
                                         std::wstring& topRight,
                                         std::wstring& bottomLeft,
                                         std::wstring& bottomCenter,
                                         std::wstring& bottomRight);
  virtual RetVal HandleJSAlert(CefRefPtr<CefBrowser> browser,
                               const std::wstring& message);
  virtual RetVal HandleJSConfirm(CefRefPtr<CefBrowser> browser,
                                 const std::wstring& message, bool& retval);
  virtual RetVal HandleJSPrompt(CefRefPtr<CefBrowser> browser,
                                const std::wstring& message,
                                const std::wstring& default_value,
                                bool& retval,
                                std::wstring& result);
};


#endif // BUILDING_CEF_SHARED
#endif // _HANDLER_CTOCPP_H
