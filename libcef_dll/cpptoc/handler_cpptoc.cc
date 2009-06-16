// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/handler_cpptoc.h"
#include "ctocpp/browser_ctocpp.h"
#include "ctocpp/frame_ctocpp.h"
#include "ctocpp/request_ctocpp.h"
#include "ctocpp/stream_ctocpp.h"
#include "ctocpp/v8value_ctocpp.h"
#include "transfer_util.h"


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

  CefWindowInfo wndInfo(*windowInfo);
  
  // |newHandler| will start off pointing to the current handler.
  CefRefPtr<CefHandler> handlerPtr = CefHandlerCppToC::Unwrap(*newHandler);
  CefHandler* origHandler = handlerPtr.get();
  
  // |parentBrowser| will be NULL if this is a top-level browser window.
  CefRefPtr<CefBrowser> browserPtr;
  if(parentBrowser)
    browserPtr = CefBrowserCToCpp::Wrap(parentBrowser);
  
  std::wstring urlStr;
  if(*url)
    urlStr = *url;

  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->HandleBeforeCreated(
      browserPtr, wndInfo, popup, handlerPtr, urlStr);

  transfer_string_contents(urlStr, url);

  if(handlerPtr.get() != origHandler) {
    // The handler has been changed.
    *newHandler = CefHandlerCppToC::Wrap(handlerPtr);
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

  return CefHandlerCppToC::Get(handler)->HandleAfterCreated(
      CefBrowserCToCpp::Wrap(browser));
}

enum cef_retval_t CEF_CALLBACK handler_handle_address_change(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    const wchar_t* url)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  if(!handler || !browser || !frame)
    return RV_CONTINUE;

  std::wstring urlStr;
  if(url)
    urlStr = url;

  return CefHandlerCppToC::Get(handler)->HandleAddressChange(
    CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame), urlStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_title_change(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    const wchar_t* title)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  std::wstring titleStr;
  if(title)
    titleStr = title;

  return CefHandlerCppToC::Get(handler)->HandleTitleChange(
      CefBrowserCToCpp::Wrap(browser), titleStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_before_browse(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    struct _cef_request_t* request, cef_handler_navtype_t navType,
    int isRedirect)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(request);
  if(!handler || !browser || !request || !frame)
    return RV_CONTINUE;

  return CefHandlerCppToC::Get(handler)->HandleBeforeBrowse(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame),
      CefRequestCToCpp::Wrap(request), navType, (isRedirect ? true : false));
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_start(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefRefPtr<CefFrame> framePtr;
  if(frame)
    framePtr = CefFrameCToCpp::Wrap(frame);

  return CefHandlerCppToC::Get(handler)->HandleLoadStart(
      CefBrowserCToCpp::Wrap(browser), framePtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_end(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  CefRefPtr<CefFrame> framePtr;
  if(frame)
    framePtr = CefFrameCToCpp::Wrap(frame);

  return CefHandlerCppToC::Get(handler)->HandleLoadEnd(
      CefBrowserCToCpp::Wrap(browser), framePtr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_load_error(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    cef_handler_errorcode_t errorCode, const wchar_t* failedUrl,
    cef_string_t* errorText)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(errorText);
  if(!handler || !browser || !errorText || !frame)
    return RV_CONTINUE;

  std::wstring failedUrlStr, errorTextStr;

  if(failedUrl)
    failedUrlStr = failedUrl;
  if(*errorText)
    errorTextStr = *errorText;

  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->HandleLoadError(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame), errorCode,
      failedUrlStr, errorTextStr);

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

  std::wstring redirectUrlStr, mimeTypeStr;
  CefRefPtr<CefStreamReader> streamPtr;

  if(*redirectUrl)
    redirectUrlStr = *redirectUrl;
  if(*mimeType)
    mimeTypeStr = *mimeType;

  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->
      HandleBeforeResourceLoad(CefBrowserCToCpp::Wrap(browser),
      CefRequestCToCpp::Wrap(request), redirectUrlStr, streamPtr, mimeTypeStr,
      loadFlags);

  transfer_string_contents(redirectUrlStr, redirectUrl);
  transfer_string_contents(mimeTypeStr, mimeType);

  if(streamPtr.get())
    *resourceStream = CefStreamReaderCToCpp::Unwrap(streamPtr);

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

  return CefHandlerCppToC::Get(handler)->HandleBeforeMenu(
      CefBrowserCToCpp::Wrap(browser), *menuInfo);
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

  std::wstring labelStr;
  if(*label)
    labelStr = *label;

  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->HandleGetMenuLabel(
      CefBrowserCToCpp::Wrap(browser), menuId, labelStr);

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

  return CefHandlerCppToC::Get(handler)->HandleMenuAction(
      CefBrowserCToCpp::Wrap(browser), menuId);
}

enum cef_retval_t CEF_CALLBACK handler_handle_print_header_footer(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    cef_print_info_t* printInfo, const wchar_t* url, const wchar_t* title,
    int currentPage, int maxPages, cef_string_t* topLeft,
    cef_string_t* topCenter, cef_string_t* topRight,
    cef_string_t* bottomLeft, cef_string_t* bottomCenter,
    cef_string_t* bottomRight)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(printInfo);
  DCHECK(topLeft && topCenter && topRight);
  DCHECK(bottomLeft && bottomCenter && bottomRight);
  if(!handler || !browser || !frame || !printInfo || !topLeft || !topCenter
      || !topRight || !bottomLeft || !bottomCenter || !bottomRight)
    return RV_CONTINUE;

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

  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->
      HandlePrintHeaderFooter(CefBrowserCToCpp::Wrap(browser),
      CefFrameCToCpp::Wrap(frame), info, urlStr, titleStr, currentPage,
      maxPages, topLeftStr, topCenterStr, topRightStr, bottomLeftStr,
      bottomCenterStr, bottomRightStr);

  transfer_string_contents(topLeftStr, topLeft);
  transfer_string_contents(topCenterStr, topCenter);
  transfer_string_contents(topRightStr, topRight);
  transfer_string_contents(bottomLeftStr, bottomLeft);
  transfer_string_contents(bottomCenterStr, bottomCenter);
  transfer_string_contents(bottomRightStr, bottomRight);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsalert(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    const wchar_t* message)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  if(!handler || !browser || !frame)
    return RV_CONTINUE;

  std::wstring messageStr;
  if(message)
    messageStr = message;

  return CefHandlerCppToC::Get(handler)->HandleJSAlert(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame), messageStr);
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsconfirm(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    const wchar_t* message, int* retval)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(retval);
  if(!handler || !browser || !retval || !frame)
    return RV_CONTINUE;

  std::wstring messageStr;
  if(message)
    messageStr = message;

  bool ret = false;
  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->HandleJSConfirm(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame), messageStr,
      ret);
  *retval = (ret ? 1 : 0);

  return rv;
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsprompt(
    struct _cef_handler_t* handler, cef_browser_t* browser, cef_frame_t* frame,
    const wchar_t* message, const wchar_t* defaultValue, int* retval,
    cef_string_t* result)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(retval);
  DCHECK(result);
  if(!handler || !browser || !frame || !retval || !result)
    return RV_CONTINUE;

  std::wstring messageStr, defaultValueStr, resultStr;

  if(message)
    messageStr = message;
  if(defaultValue)
    defaultValueStr = defaultValue;
  if(*result)
    resultStr = *result;

  bool ret = false;
  enum cef_retval_t rv = CefHandlerCppToC::Get(handler)->HandleJSPrompt(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame), messageStr,
      defaultValueStr, ret, resultStr);
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

  return CefHandlerCppToC::Get(handler)->HandleBeforeWindowClose(
      CefBrowserCToCpp::Wrap(browser));
}

enum cef_retval_t CEF_CALLBACK handler_handle_take_focus(
    struct _cef_handler_t* handler, cef_browser_t* browser, int reverse)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  return CefHandlerCppToC::Get(handler)->HandleTakeFocus(
      CefBrowserCToCpp::Wrap(browser), (reverse ? true : false));
}

enum cef_retval_t CEF_CALLBACK handler_handle_jsbinding(
    struct _cef_handler_t* handler, cef_browser_t* browser,
    cef_frame_t* frame, struct _cef_v8value_t* object)
{
  DCHECK(handler);
  DCHECK(browser);
  DCHECK(frame);
  DCHECK(object);
  if(!handler || !browser || !frame || !object)
    return RV_CONTINUE;

  return CefHandlerCppToC::Get(handler)->HandleJSBinding(
      CefBrowserCToCpp::Wrap(browser), CefFrameCToCpp::Wrap(frame),
      CefV8ValueCToCpp::Wrap(object));
}

enum cef_retval_t CEF_CALLBACK handler_handle_set_focus(
    struct _cef_handler_t* handler, cef_browser_t* browser, int isWidget)
{
  DCHECK(handler);
  DCHECK(browser);
  if(!handler || !browser)
    return RV_CONTINUE;

  return CefHandlerCppToC::Get(handler)->HandleSetFocus(
      CefBrowserCToCpp::Wrap(browser), isWidget);
}


CefHandlerCppToC::CefHandlerCppToC(CefHandler* cls)
    : CefCppToC<CefHandlerCppToC, CefHandler, cef_handler_t>(cls)
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
  struct_.struct_.handle_jsbinding = handler_handle_jsbinding;
  struct_.struct_.handle_set_focus = handler_handle_set_focus;
}

#ifdef _DEBUG
long CefCppToC<CefHandlerCppToC, CefHandler, cef_handler_t>::DebugObjCt = 0;
#endif
