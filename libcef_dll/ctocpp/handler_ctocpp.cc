// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_cpptoc.h"
#include "ctocpp/handler_ctocpp.h"
#include "transfer_util.h"
#include "base/logging.h"


CefHandler::RetVal CefHandlerCToCpp::HandleBeforeCreated(
    CefRefPtr<CefBrowser> parentBrowser, CefWindowInfo& windowInfo, bool popup,
    CefRefPtr<CefHandler>& handler, std::wstring& url)
{
  if(CEF_MEMBER_MISSING(struct_, handle_before_created))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = NULL;
  cef_browser_t* browserStructPtr = NULL;
  if(parentBrowser.get()) {
    browserPtr = new CefBrowserCppToC(parentBrowser);
    browserPtr->AddRef();
    browserStructPtr = browserPtr->GetStruct();
  }

  CefHandlerCToCpp* handlerPtr = NULL;
  cef_handler_t* handlerRet = NULL;
  if(handler.get()) {
    handlerPtr = static_cast<CefHandlerCToCpp*>(handler.get());
    handlerPtr->UnderlyingAddRef();
    handlerRet = handlerPtr->GetStruct();
  }

  cef_string_t urlRet = NULL;
  if(!url.empty())
    urlRet = cef_string_alloc(url.c_str());

  cef_retval_t rv = struct_->handle_before_created(struct_,
    browserStructPtr, &windowInfo, popup, &handlerRet, &urlRet);

  if(handlerPtr && handlerRet != handlerPtr->GetStruct()) {
    // The handler was changed.
    if(handlerRet) {
      CefHandlerCToCpp* hp = new CefHandlerCToCpp(handlerRet);
      handler = hp;
      hp->UnderlyingRelease();
    } else {
      handler = NULL;
    }
  }

  transfer_string_contents(urlRet, url, true);

  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleAfterCreated(
    CefRefPtr<CefBrowser> browser)
{
  if(CEF_MEMBER_MISSING(struct_, handle_after_created))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();
  return struct_->handle_after_created(struct_, browserPtr->GetStruct());
}

CefHandler::RetVal CefHandlerCToCpp::HandleAddressChange(
    CefRefPtr<CefBrowser> browser, const std::wstring& url)
{
  if(CEF_MEMBER_MISSING(struct_, handle_address_change))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();
  return struct_->handle_address_change(struct_, browserPtr->GetStruct(),
      url.c_str());
}

CefHandler::RetVal CefHandlerCToCpp::HandleTitleChange(
    CefRefPtr<CefBrowser> browser, const std::wstring& title)
{
  if(CEF_MEMBER_MISSING(struct_, handle_title_change))
   return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();
  return struct_->handle_title_change(struct_, browserPtr->GetStruct(),
      title.c_str());
}

CefHandler::RetVal CefHandlerCToCpp::HandleBeforeBrowse(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefRequest> request,
    NavType navType, bool isRedirect)
{
  if(CEF_MEMBER_MISSING(struct_, handle_before_browse))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  CefRequestCppToC* requestPtr = new CefRequestCppToC(request);
  requestPtr->AddRef();

  return struct_->handle_before_browse(struct_, browserPtr->GetStruct(),
    requestPtr->GetStruct(), navType, isRedirect);
}

CefHandler::RetVal CefHandlerCToCpp::HandleLoadStart(
    CefRefPtr<CefBrowser> browser)
{
  if(CEF_MEMBER_MISSING(struct_, handle_load_start))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->handle_load_start(struct_, browserPtr->GetStruct());
}

CefHandler::RetVal CefHandlerCToCpp::HandleLoadEnd(
    CefRefPtr<CefBrowser> browser)
{
  if(CEF_MEMBER_MISSING(struct_, handle_load_end))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->handle_load_end(struct_, browserPtr->GetStruct());
}

CefHandler::RetVal CefHandlerCToCpp::HandleLoadError(
    CefRefPtr<CefBrowser> browser, ErrorCode errorCode,
    const std::wstring& failedUrl, std::wstring& errorText)
{
  if(CEF_MEMBER_MISSING(struct_, handle_load_error))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  cef_string_t errorTextRet = NULL;
  if(!errorText.empty())
    errorTextRet = cef_string_alloc(errorText.c_str());

  cef_retval_t rv = struct_->handle_load_error(struct_, browserPtr->GetStruct(),
      errorCode, failedUrl.c_str(), &errorTextRet);

  transfer_string_contents(errorTextRet, errorText, true);

  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefRequest> request,
    std::wstring& redirectUrl, CefRefPtr<CefStreamReader>& resourceStream,
    std::wstring& mimeType, int loadFlags)
{
  if(CEF_MEMBER_MISSING(struct_, handle_before_resource_load))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  CefRequestCppToC* requestPtr = new CefRequestCppToC(request);
  requestPtr->AddRef();

  cef_string_t redirectUrlRet = NULL;
  cef_string_t mimeTypeRet = NULL;
  cef_stream_reader_t* streamRet = NULL;

  if(!redirectUrl.empty())
    redirectUrlRet = cef_string_alloc(redirectUrl.c_str());
  
  cef_retval_t rv = struct_->handle_before_resource_load(struct_,
      browserPtr->GetStruct(), requestPtr->GetStruct(), &redirectUrlRet,
      &streamRet, &mimeTypeRet, loadFlags);

  transfer_string_contents(redirectUrlRet, redirectUrl, true);
  transfer_string_contents(mimeTypeRet, mimeType, true);

  if(streamRet) {
    CefStreamReaderCppToC::Struct* sp =
        reinterpret_cast<CefStreamReaderCppToC::Struct*>(streamRet);
    resourceStream = sp->class_->GetClass();
    sp->class_->Release();
  }

  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleBeforeMenu(
    CefRefPtr<CefBrowser> browser, const MenuInfo& menuInfo)
{
  if(CEF_MEMBER_MISSING(struct_, handle_before_menu))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->handle_before_menu(struct_, browserPtr->GetStruct(),
      &menuInfo);
}

CefHandler::RetVal CefHandlerCToCpp::HandleGetMenuLabel(
    CefRefPtr<CefBrowser> browser, MenuId menuId, std::wstring& label)
{
  if(CEF_MEMBER_MISSING(struct_, handle_get_menu_label))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  cef_string_t labelRet = NULL;
  if(!label.empty())
    labelRet = cef_string_alloc(label.c_str());

  cef_retval_t rv = struct_->handle_get_menu_label(struct_,
      browserPtr->GetStruct(), menuId, &labelRet);

  transfer_string_contents(labelRet, label, true);

  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleMenuAction(
    CefRefPtr<CefBrowser> browser, MenuId menuId)
{
  if(CEF_MEMBER_MISSING(struct_, handle_menu_action))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->handle_menu_action(struct_, browserPtr->GetStruct(), menuId);
}

CefHandler::RetVal CefHandlerCToCpp::HandlePrintHeaderFooter(
    CefRefPtr<CefBrowser> browser, CefPrintInfo& printInfo,
    const std::wstring& url, const std::wstring& title, int currentPage,
    int maxPages, std::wstring& topLeft, std::wstring& topCenter,
    std::wstring& topRight, std::wstring& bottomLeft,
    std::wstring& bottomCenter, std::wstring& bottomRight)
{
  if(CEF_MEMBER_MISSING(struct_, handle_print_header_footer))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  cef_string_t topLeftRet = NULL, topCenterRet = NULL, topRightRet = NULL,
      bottomLeftRet = NULL, bottomCenterRet = NULL, bottomRightRet = NULL;

  if(!topLeft.empty())
    topLeftRet = cef_string_alloc(topLeft.c_str());
  if(!topCenter.empty())
    topCenterRet = cef_string_alloc(topCenter.c_str());
  if(!topRight.empty())
    topRightRet = cef_string_alloc(topRight.c_str());
  if(!bottomLeft.empty())
    bottomLeftRet = cef_string_alloc(bottomLeft.c_str());
  if(!bottomCenter.empty())
    bottomCenterRet = cef_string_alloc(bottomCenter.c_str());
  if(!bottomRight.empty())
    bottomRightRet = cef_string_alloc(bottomRight.c_str());

  cef_retval_t rv = struct_->handle_print_header_footer(struct_,
      browserPtr->GetStruct(), &printInfo, url.c_str(), title.c_str(),
      currentPage, maxPages, &topLeftRet, &topCenterRet, &topRightRet,
      &bottomLeftRet, &bottomCenterRet, &bottomRightRet);

  transfer_string_contents(topLeftRet, topLeft, true);
  transfer_string_contents(topCenterRet, topCenter, true);
  transfer_string_contents(topRightRet, topRight, true);
  transfer_string_contents(bottomLeftRet, bottomLeft, true);
  transfer_string_contents(bottomCenterRet, bottomCenter, true);
  transfer_string_contents(bottomRightRet, bottomRight, true);

  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleJSAlert(
    CefRefPtr<CefBrowser> browser, const std::wstring& message)
{
  if(CEF_MEMBER_MISSING(struct_, handle_jsalert))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  return struct_->handle_jsalert(struct_, browserPtr->GetStruct(),
      message.c_str());
}

CefHandler::RetVal CefHandlerCToCpp::HandleJSConfirm(
    CefRefPtr<CefBrowser> browser, const std::wstring& message, bool& retval)
{
  if(CEF_MEMBER_MISSING(struct_, handle_jsconfirm))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  int ret = 0;
  cef_retval_t rv = struct_->handle_jsconfirm(struct_, browserPtr->GetStruct(),
      message.c_str(), &ret);
  retval = (ret ? true : false);
  return rv;
}

CefHandler::RetVal CefHandlerCToCpp::HandleJSPrompt(
    CefRefPtr<CefBrowser> browser, const std::wstring& message,
    const std::wstring& defaultValue, bool& retval, std::wstring& result)
{
  if(CEF_MEMBER_MISSING(struct_, handle_jsprompt))
    return RV_CONTINUE;

  CefBrowserCppToC* browserPtr = new CefBrowserCppToC(browser);
  browserPtr->AddRef();

  cef_string_t resultRet = NULL;
  if(!result.empty())
    resultRet = cef_string_alloc(result.c_str());

  int ret = 0;
  cef_retval_t rv = struct_->handle_jsprompt(struct_, browserPtr->GetStruct(),
      message.c_str(), defaultValue.c_str(), &ret, &resultRet);
  retval = (ret ? true : false);

  transfer_string_contents(resultRet, result, true);

  return rv;
}
