// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "cpptoc/browser_cpptoc.h"
#include "cpptoc/request_cpptoc.h"
#include "cpptoc/stream_cpptoc.h"
#include "ctocpp/handler_ctocpp.h"
#include "ctocpp/jshandler_ctocpp.h"
#include "base/logging.h"


int CEF_CALLBACK browser_can_go_back(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  return impl->class_->GetClass()->CanGoBack();
}

void CEF_CALLBACK browser_go_back(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->GoBack();
}

int CEF_CALLBACK browser_can_go_forward(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  return impl->class_->GetClass()->CanGoForward();
}

void CEF_CALLBACK browser_go_forward(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->GoForward();
}

void CEF_CALLBACK browser_reload(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Reload();
}

void CEF_CALLBACK browser_stop_load(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->StopLoad();
}

void CEF_CALLBACK browser_undo(cef_browser_t* browser,
                               enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Undo(targetFrame);
}

void CEF_CALLBACK browser_redo(cef_browser_t* browser,
                               enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Redo(targetFrame);
}

void CEF_CALLBACK browser_cut(cef_browser_t* browser,
                              enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Cut(targetFrame);
}

void CEF_CALLBACK browser_copy(cef_browser_t* browser,
                               enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Copy(targetFrame);
}

void CEF_CALLBACK browser_paste(cef_browser_t* browser,
                                enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Paste(targetFrame);
}

void CEF_CALLBACK browser_delete(cef_browser_t* browser,
                                 enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Delete(targetFrame);
}

void CEF_CALLBACK browser_select_all(cef_browser_t* browser,
                                     enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->SelectAll(targetFrame);
}

void CEF_CALLBACK browser_set_focus(struct _cef_browser_t* browser, int enable)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->SetFocus(enable ? true : false);
}

void CEF_CALLBACK browser_print(cef_browser_t* browser,
                                enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->Print(targetFrame);
}

void CEF_CALLBACK browser_view_source(cef_browser_t* browser,
                                      enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  impl->class_->GetClass()->ViewSource(targetFrame);
}

cef_string_t CEF_CALLBACK browser_get_source(cef_browser_t* browser,
                                             enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);

  std::wstring sourceStr = impl->class_->GetClass()->GetSource(targetFrame);
  if(sourceStr.empty())
    return NULL;
  return cef_string_alloc(sourceStr.c_str());
}

cef_string_t CEF_CALLBACK browser_get_text(cef_browser_t* browser,
                                           enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring textStr = impl->class_->GetClass()->GetText(targetFrame);
  if(textStr.empty())
    return NULL;
  return cef_string_alloc(textStr.c_str());
}

void CEF_CALLBACK browser_load_request(cef_browser_t* browser,
                                       cef_request_t* request)
{
  DCHECK(browser);
  DCHECK(request);
  if(!browser || !request)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  CefRequestCppToC::Struct* structPtr =
      reinterpret_cast<CefRequestCppToC::Struct*>(request);
  CefRefPtr<CefRequest> requestPtr(structPtr->class_->GetClass());
  structPtr->class_->Release();
  impl->class_->GetClass()->LoadRequest(requestPtr);
}

void CEF_CALLBACK browser_load_url(cef_browser_t* browser, const wchar_t* url,
                                   const wchar_t* frame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring urlStr, frameStr;
  if(url)
    urlStr = url;
  if(frame)
    frameStr = frame;
  impl->class_->GetClass()->LoadURL(urlStr, frameStr);
}

void CEF_CALLBACK browser_load_string(cef_browser_t* browser,
                                      const wchar_t* string,
                                      const wchar_t* url)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring stringStr, urlStr;
  if(string)
    stringStr = string;
  if(url)
    urlStr = url;
  impl->class_->GetClass()->LoadString(stringStr, urlStr);
}

void CEF_CALLBACK browser_load_stream(cef_browser_t* browser,
                                      cef_stream_reader_t* stream,
                                      const wchar_t* url)
{
  DCHECK(browser);
  DCHECK(stream);
  if(!browser || !stream)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  CefStreamReaderCppToC::Struct* structPtr =
      reinterpret_cast<CefStreamReaderCppToC::Struct*>(stream);
  CefRefPtr<CefStreamReader> streamPtr(structPtr->class_->GetClass());
  structPtr->class_->Release();
  
  std::wstring urlStr;
  if(url)
    urlStr = url;
  impl->class_->GetClass()->LoadStream(streamPtr, urlStr);
}

void CEF_CALLBACK browser_execute_javascript(cef_browser_t* browser,
                                             const wchar_t* jsCode,
                                             const wchar_t* scriptUrl,
                                             int startLine,
                                             enum cef_targetframe_t targetFrame)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring jsCodeStr, scriptUrlStr;
  if(jsCode)
    jsCodeStr = jsCode;
  if(scriptUrl)
    scriptUrlStr = scriptUrl;

  impl->class_->GetClass()->ExecuteJavaScript(jsCodeStr, scriptUrlStr,
      startLine, targetFrame);
}

int CEF_CALLBACK browser_add_jshandler(cef_browser_t* browser,
                                       const wchar_t* classname,
                                       cef_jshandler_t* handler)
{
  DCHECK(browser);
  DCHECK(handler);
  if(!browser || !handler)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  CefJSHandlerCToCpp* hp = new CefJSHandlerCToCpp(handler);
  CefRefPtr<CefJSHandler> handlerPtr(hp);
  hp->UnderlyingRelease();
  
  std::wstring classnameStr;
  if(classname)
    classnameStr = classname;
  return impl->class_->GetClass()->AddJSHandler(classnameStr, handlerPtr);
}

int CEF_CALLBACK browser_has_jshandler(cef_browser_t* browser,
                                       const wchar_t* classname)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring classnameStr;
  if(classname)
    classnameStr = classname;
  return impl->class_->GetClass()->HasJSHandler(classnameStr);
}

cef_jshandler_t* CEF_CALLBACK browser_get_jshandler(cef_browser_t* browser,
                                                    const wchar_t* classname)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring classnameStr;
  if(classname)
    classnameStr = classname;
  CefRefPtr<CefJSHandler> handler =
      impl->class_->GetClass()->GetJSHandler(classnameStr);
  if(handler.get()) {
    CefJSHandlerCToCpp* hp = static_cast<CefJSHandlerCToCpp*>(handler.get());
    hp->UnderlyingAddRef();
    return hp->GetStruct();
  }
  return NULL;
}

int CEF_CALLBACK browser_remove_jshandler(cef_browser_t* browser,
                                          const wchar_t* classname)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring classnameStr;
  if(classname)
    classnameStr = classname;
  return impl->class_->GetClass()->RemoveJSHandler(classnameStr);
}

void CEF_CALLBACK browser_remove_all_jshandlers(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  return impl->class_->GetClass()->RemoveAllJSHandlers();
}

cef_window_handle_t CEF_CALLBACK browser_get_window_handle(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  return impl->class_->GetClass()->GetWindowHandle();
}

int CEF_CALLBACK browser_is_popup(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return 0;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  return impl->class_->GetClass()->IsPopup();
}

cef_handler_t* CEF_CALLBACK browser_get_handler(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  CefRefPtr<CefHandler> handler = impl->class_->GetClass()->GetHandler();
  if(handler.get()) {
    CefHandlerCToCpp* hp = static_cast<CefHandlerCToCpp*>(handler.get());
    hp->UnderlyingAddRef();
    return hp->GetStruct();
  }
  return NULL;
}

cef_string_t CEF_CALLBACK browser_get_url(cef_browser_t* browser)
{
  DCHECK(browser);
  if(!browser)
    return NULL;

  CefBrowserCppToC::Struct* impl =
      reinterpret_cast<CefBrowserCppToC::Struct*>(browser);
  
  std::wstring urlStr = impl->class_->GetClass()->GetURL();
  if(urlStr.empty())
    return NULL;
  return cef_string_alloc(urlStr.c_str());
}

CefBrowserCppToC::CefBrowserCppToC(CefBrowser* cls)
    : CefCppToC<CefBrowser, cef_browser_t>(cls)
{
  struct_.struct_.can_go_back = browser_can_go_back;
  struct_.struct_.go_back = browser_go_back;
  struct_.struct_.can_go_forward = browser_can_go_forward;
  struct_.struct_.go_forward = browser_go_forward;
  struct_.struct_.reload = browser_reload;
  struct_.struct_.stop_load = browser_stop_load;
  struct_.struct_.undo = browser_undo;
  struct_.struct_.redo = browser_redo;
  struct_.struct_.cut = browser_cut;
  struct_.struct_.copy = browser_copy;
  struct_.struct_.paste = browser_paste;
  struct_.struct_.del = browser_delete;
  struct_.struct_.select_all = browser_select_all;
  struct_.struct_.set_focus = browser_set_focus;
  struct_.struct_.print = browser_print;
  struct_.struct_.view_source = browser_view_source;
  struct_.struct_.get_source = browser_get_source;
  struct_.struct_.get_text = browser_get_text;
  struct_.struct_.load_request = browser_load_request;
  struct_.struct_.load_url = browser_load_url;
  struct_.struct_.load_string = browser_load_string;
  struct_.struct_.load_stream = browser_load_stream;
  struct_.struct_.execute_javascript = browser_execute_javascript;
  struct_.struct_.add_jshandler = browser_add_jshandler;
  struct_.struct_.has_jshandler = browser_has_jshandler;
  struct_.struct_.get_jshandler = browser_get_jshandler;
  struct_.struct_.remove_jshandler = browser_remove_jshandler;
  struct_.struct_.remove_all_jshandlers = browser_remove_all_jshandlers;
  struct_.struct_.get_window_handle = browser_get_window_handle;
  struct_.struct_.is_popup = browser_is_popup;
  struct_.struct_.get_handler = browser_get_handler;
  struct_.struct_.get_url = browser_get_url;
}

#ifdef _DEBUG
long CefCppToC<CefBrowser, cef_browser_t>::DebugObjCt = 0;
#endif
