// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/frame_cpptoc.h"
#include "ctocpp/handler_ctocpp.h"


int CEF_CALLBACK browser_can_go_back(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  return CefBrowserCppToC::Get(browser)->CanGoBack();
}

void CEF_CALLBACK browser_go_back(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Get(browser)->GoBack();
}

int CEF_CALLBACK browser_can_go_forward(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  return CefBrowserCppToC::Get(browser)->CanGoForward();
}

void CEF_CALLBACK browser_go_forward(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Get(browser)->GoForward();
}

void CEF_CALLBACK browser_reload(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Get(browser)->Reload();
}

void CEF_CALLBACK browser_stop_load(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Get(browser)->StopLoad();
}

void CEF_CALLBACK browser_set_focus(struct _cef_browser_t* browser, int enable)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Get(browser)->SetFocus(enable ? true : false);
}

cef_window_handle_t CEF_CALLBACK browser_get_window_handle(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  return CefBrowserCppToC::Get(browser)->GetWindowHandle();
}

int CEF_CALLBACK browser_is_popup(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  return CefBrowserCppToC::Get(browser)->IsPopup();
}

cef_handler_t* CEF_CALLBACK browser_get_handler(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefRefPtr<CefBrowser> browserPtr = CefBrowserCppToC::Get(browser);
  CefRefPtr<CefHandler> handlerPtr = browserPtr->GetHandler();
  if(handlerPtr.get())
    return CefHandlerCToCpp::Unwrap(handlerPtr);

  return NULL;
}

struct _cef_frame_t* CEF_CALLBACK browser_get_main_frame(
    struct _cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefRefPtr<CefBrowser> browserPtr = CefBrowserCppToC::Get(browser);
  CefRefPtr<CefFrame> framePtr = browserPtr->GetMainFrame();
  if(framePtr.get())
    return CefFrameCppToC::Wrap(framePtr);
  return NULL;    
}

struct _cef_frame_t* CEF_CALLBACK browser_get_focused_frame(
    struct _cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefRefPtr<CefBrowser> browserPtr = CefBrowserCppToC::Get(browser);
  CefRefPtr<CefFrame> framePtr = browserPtr->GetFocusedFrame();
  if(framePtr.get())
    return CefFrameCppToC::Wrap(framePtr);
  return NULL; 
}

struct _cef_frame_t* CEF_CALLBACK browser_get_frame(
    struct _cef_browser_t* browser, const wchar_t* name)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  std::wstring nameStr;
  if(name)
    nameStr = name;
  if(nameStr.empty())
    return NULL;

  CefRefPtr<CefBrowser> browserPtr = CefBrowserCppToC::Get(browser);
  CefRefPtr<CefFrame> framePtr = browserPtr->GetFrame(nameStr);
  if(framePtr.get())
    return CefFrameCppToC::Wrap(framePtr);
  return NULL;
}

size_t CEF_CALLBACK browser_get_frame_names(struct _cef_browser_t* browser,
                                            cef_string_list_t list)
{
  DCHECK(browser);
  DCHECK(list);
  if(!browser || !list)
    return NULL;

  CefRefPtr<CefBrowser> browserPtr = CefBrowserCppToC::Get(browser);
  std::vector<std::wstring> stringList;
  browserPtr->GetFrameNames(stringList);
  size_t size = stringList.size();
  for(size_t i = 0; i < size; ++i)
    cef_string_list_append(list, stringList[i].c_str());
  return size;
}

CefBrowserCppToC::CefBrowserCppToC(CefBrowser* cls)
    : CefCppToC<CefBrowserCppToC, CefBrowser, cef_browser_t>(cls)
{
  struct_.struct_.can_go_back = browser_can_go_back;
  struct_.struct_.go_back = browser_go_back;
  struct_.struct_.can_go_forward = browser_can_go_forward;
  struct_.struct_.go_forward = browser_go_forward;
  struct_.struct_.reload = browser_reload;
  struct_.struct_.stop_load = browser_stop_load;
  struct_.struct_.set_focus = browser_set_focus;
  struct_.struct_.get_window_handle = browser_get_window_handle;
  struct_.struct_.is_popup = browser_is_popup;
  struct_.struct_.get_handler = browser_get_handler;
  struct_.struct_.get_main_frame = browser_get_main_frame;
  struct_.struct_.get_focused_frame = browser_get_focused_frame;
  struct_.struct_.get_frame = browser_get_frame;
  struct_.struct_.get_frame_names = browser_get_frame_names;
}

#ifdef _DEBUG
long CefCppToC<CefBrowserCppToC, CefBrowser, cef_browser_t>::DebugObjCt = 0;
#endif
