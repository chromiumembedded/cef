// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/handler_cpptoc.h"
#include "ctocpp/browser_ctocpp.h"
#include "ctocpp/request_ctocpp.h"
#include "ctocpp/stream_ctocpp.h"
#include "transfer_util.h"
#include "base/logging.h"


enum cef_retval_t CEF_CALLBACK handler_handle_before_created(
    struct _cef_handler_t* handler, cef_browser_t* parentBrowser,
    cef_window_info_t* windowInfo, int popup,
    struct _cef_handler_t** newHandler, cef_string_t* url)
{
  DCHECK(handler);
  DCHECK(windowInfo);
  DCHECK(newHandler && *newHandler);
  DCHECK(url);
  if(!handler || !windowInfo || !newHandler || !*newHandler || !url)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  // |newHandler| will start off pointing to the current handler.
  CefHandlerCppToC::Struct* structPtr
      = reinterpret_cast<CefHandlerCppToC::Struct*>(*newHandler);
  
  CefWindowInfo wndInfo(*windowInfo);
  CefRefPtr<CefBrowser> browserPtr;
  CefRefPtr<CefHandler> handlerPtr(structPtr->class_->GetClass());
  structPtr->class_->Release();

  std::wstring urlStr;
  
  // |parentBrowser| will be NULL if this is a top-level browser window.
  if(parentBrowser)
  {
    CefBrowserCToCpp* bp = new CefBrowserCToCpp(parentBrowser);
    browserPtr = bp;
    bp->UnderlyingRelease();
  }
  
  if(*url)
    urlStr = *url;

  enum cef_retval_t rv = impl->class_->GetClass()->HandleBeforeCreated(
      browserPtr, wndInfo, popup, handlerPtr, urlStr);

  transfer_string_contents(urlStr, url);

  if(handlerPtr.get() != structPtr->class_->GetClass())
  {
    // The handler has been changed.
    CefHandlerCppToC* hobj = new CefHandlerCppToC(handlerPtr);
    hobj->AddRef();
    *newHandler = hobj->GetStruct();
  }

  // WindowInfo may or may not have changed.
  *windowInfo = wndInfo;
#ifdef WIN32
  // The m_windowName must be duplicated since it's a cef_string_t
  if(windowInfo->m_windowName)
    windowInfo->m_windowName = cef_string_alloc(windowInfo->m_windowName);
#endif

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_after_created(
    struct _cef_handler_t* handler, cef_browser_t* browser)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  return impl->class_->GetClass()->HandleAfterCreated(browserPtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_address_change(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* url)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring urlStr;
  if(url)
    urlStr = url;

  return impl->class_->GetClass()->HandleAddressChange(browserPtr, urlStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_title_change(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* title)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring titleStr;
  if(title)
    titleStr = title;

  return impl->class_->GetClass()->HandleTitleChange(browserPtr, titleStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_before_browse(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    struct _cef_request_t* request, cef_handler_navtype_t navType,
    int isRedirect)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(request);
  if(!handler || !browser || !request)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  CefRequestCToCpp* rp = new CefRequestCToCpp(request);
  CefRefPtr<CefRequest> requestPtr(rp);
  rp->UnderlyingRelease();

  return impl->class_->GetClass()->HandleBeforeBrowse(browserPtr, requestPtr,
      navType, (isRedirect ? true : false));
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_start(
    struct _cef_handler_t* handler, cef_browser_t* browser)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  return impl->class_->GetClass()->HandleLoadStart(browserPtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_end(
    struct _cef_handler_t* handler, cef_browser_t* browser)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  return impl->class_->GetClass()->HandleLoadEnd(browserPtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_error(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    cef_handler_errorcode_t errorCode, const wchar_t* failedUrl,
    cef_string_t* errorText)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(errorText);
  if(!handler || !browser || !errorText)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring failedUrlStr, errorTextStr;

  if(failedUrl)
    failedUrlStr = failedUrl;
  if(*errorText)
    errorTextStr = *errorText;

  enum cef_retval_t rv = impl->class_->GetClass()->HandleLoadError(browserPtr,
      errorCode, failedUrlStr, errorTextStr);

  transfer_string_contents(errorTextStr, errorText);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_before_resource_load(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    struct _cef_request_t* request, cef_string_t* redirectUrl,
    struct _cef_stream_reader_t** resourceStream, cef_string_t* mimeType,
    int loadFlags)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(redirectUrl);
  DCHECK(resourceStream);
  DCHECK(mimeType);
  if(!handler || !browser || !redirectUrl || !resourceStream || !mimeType)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  CefRequestCToCpp* rp = new CefRequestCToCpp(request);
  CefRefPtr<CefRequest> requestPtr(rp);
  rp->UnderlyingRelease();

  std::wstring redirectUrlStr, mimeTypeStr;
  CefRefPtr<CefStreamReader> streamPtr;

  if(*redirectUrl)
    redirectUrlStr = *redirectUrl;
  if(*mimeType)
    mimeTypeStr = *mimeType;

  enum cef_retval_t rv = impl->class_->GetClass()->HandleBeforeResourceLoad(
      browserPtr, requestPtr, redirectUrlStr, streamPtr, mimeTypeStr,
      loadFlags);

  transfer_string_contents(redirectUrlStr, redirectUrl);
  transfer_string_contents(mimeTypeStr, mimeType);

  if(streamPtr.get())
  {
    CefStreamReaderCToCpp* sp =
        static_cast<CefStreamReaderCToCpp*>(streamPtr.get());
    sp->UnderlyingAddRef();
    *resourceStream = sp->GetStruct();
  }

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_before_menu(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const cef_handler_menuinfo_t* menuInfo)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(menuInfo);
  if(!handler || !browser || !menuInfo)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  return impl->class_->GetClass()->HandleBeforeMenu(browserPtr, *menuInfo);
}

enum cef_retval_t CEF_CALLBACK handler_handle_get_menu_label(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    cef_handler_menuid_t menuId, cef_string_t* label)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(label);
  if(!handler || !browser || !label)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring labelStr;
  if(*label)
    labelStr = *label;

  enum cef_retval_t rv = impl->class_->GetClass()->HandleGetMenuLabel(
      browserPtr, menuId, labelStr);

  transfer_string_contents(labelStr, label);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_menu_action(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    cef_handler_menuid_t menuId)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  return impl->class_->GetClass()->HandleMenuAction(browserPtr, menuId);
}

enum cef_retval_t CEF_CALLBACK handler_handle_print_header_footer(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    cef_print_info_t* printInfo, const wchar_t* url, const wchar_t* title,
    int currentPage, int maxPages, cef_string_t* topLeft,
    cef_string_t* topCenter, cef_string_t* topRight,
    cef_string_t* bottomLeft, cef_string_t* bottomCenter,
    cef_string_t* bottomRight)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(printInfo);
  DCHECK(topLeft && topCenter && topRight);
  DCHECK(bottomLeft && bottomCenter && bottomRight);
  if(!handler || !browser || !printInfo || !topLeft || !topCenter || !topRight
      || !bottomLeft || !bottomCenter || !bottomRight)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring urlStr, titleStr;
  std::wstring topLeftStr, topCenterStr, topRightStr;
  std::wstring bottomLeftStr, bottomCenterStr, bottomRightStr;
  CefPrintInfo info = *printInfo;

  if(url)
    urlStr = url;
  if(title)
    titleStr = title;
  if(*topLeft)
    topLeftStr = *topLeft;
  if(*topCenter)
    topCenterStr = *topCenter;
  if(*topRight)
    topRightStr = *topRight;
  if(*bottomLeft)
    bottomLeftStr = *bottomLeft;
  if(*bottomCenter)
    bottomCenterStr = *bottomCenter;
  if(*bottomRight)
    bottomRightStr = *bottomRight;

  enum cef_retval_t rv = impl->class_->GetClass()->
      HandlePrintHeaderFooter(browserPtr, info, urlStr, titleStr,
          currentPage, maxPages, topLeftStr, topCenterStr, topRightStr,
          bottomLeftStr, bottomCenterStr, bottomRightStr);

  transfer_string_contents(topLeftStr, topLeft);
  transfer_string_contents(topCenterStr, topCenter);
  transfer_string_contents(topRightStr, topRight);
  transfer_string_contents(bottomLeftStr, bottomLeft);
  transfer_string_contents(bottomCenterStr, bottomCenter);
  transfer_string_contents(bottomRightStr, bottomRight);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsalert(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* message)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring messageStr;
  if(message)
    messageStr = message;

  return impl->class_->GetClass()->HandleJSAlert(browserPtr, messageStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsconfirm(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* message, int* retval)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(retval);
  if(!handler || !browser || !retval)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring messageStr;
  if(message)
    messageStr = message;

  bool ret = false;
  enum cef_retval_t rv = impl->class_->GetClass()->HandleJSConfirm(browserPtr,
      messageStr, ret);
  *retval = (ret ? 1 : 0);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsprompt(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* message, const wchar_t* defaultValue, int* retval,
    cef_string_t* result)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(retval);
  DCHECK(result);
  if(!handler || !browser || !retval || !result)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();

  std::wstring messageStr, defaultValueStr, resultStr;

  if(message)
    messageStr = message;
  if(defaultValue)
    defaultValueStr = defaultValue;
  if(*result)
    resultStr = *result;

  bool ret = false;
  enum cef_retval_t rv = impl->class_->GetClass()->HandleJSPrompt(
      browserPtr, messageStr, defaultValueStr, ret, resultStr);
  *retval = (ret ? 1 : 0);

  transfer_string_contents(resultStr, result);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_before_window_close(
    struct _cef_handler_t* handler, cef_browser_t* browser)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  return impl->class_->GetClass()->HandleBeforeWindowClose(browserPtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_take_focus(
    struct _cef_handler_t* handler, cef_browser_t* browser, int reverse)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefHandlerCppToC::Struct* impl =
      reinterpret_cast<CefHandlerCppToC::Struct*>(handler);
  
  CefBrowserCToCpp* bp = new CefBrowserCToCpp(browser);
  CefRefPtr<CefBrowser> browserPtr(bp);
  bp->UnderlyingRelease();
  
  return impl->class_->GetClass()->
      HandleTakeFocus(browserPtr, (reverse ? true : false));
}


CefHandlerCppToC::CefHandlerCppToC(CefHandler* cls)
    : CefCppToC<CefHandler, cef_handler_t>(cls)
{
  struct_.struct_.handle_before_created = handler_handle_before_created;
  struct_.struct_.handle_after_created = handler_handle_after_created;
  struct_.struct_.handle_address_change = handler_handle_address_change;
  struct_.struct_.handle_title_change = handler_handle_title_change;
  struct_.struct_.handle_before_browse = handler_handle_before_browse;
  struct_.struct_.handle_load_start = handler_handle_load_start;
  struct_.struct_.handle_load_end = handler_handle_load_end;
  struct_.struct_.handle_load_error = handler_handle_load_error;
  struct_.struct_.handle_before_resource_load =
      handler_handle_before_resource_load;
  struct_.struct_.handle_before_menu = handler_handle_before_menu;
  struct_.struct_.handle_get_menu_label = handler_handle_get_menu_label;
  struct_.struct_.handle_menu_action = handler_handle_menu_action;
  struct_.struct_.handle_print_header_footer =
      handler_handle_print_header_footer;
  struct_.struct_.handle_jsalert = handler_handle_jsalert;
  struct_.struct_.handle_jsconfirm = handler_handle_jsconfirm;
  struct_.struct_.handle_jsprompt = handler_handle_jsprompt;
  struct_.struct_.handle_before_window_close =
      handler_handle_before_window_close;
  struct_.struct_.handle_take_focus = handler_handle_take_focus;
}

#ifdef _DEBUG
long CefCppToC<CefHandler, cef_handler_t>::DebugObjCt = 0;
#endif
