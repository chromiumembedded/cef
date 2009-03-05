// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "../precompiled_libcef.h"
#include "ctocpp/browser_ctocpp.h"
#include "ctocpp/request_ctocpp.h"
#include "ctocpp/stream_ctocpp.h"
#include "cpptoc/handler_cpptoc.h"
#include "cpptoc/jshandler_cpptoc.h"
#include "base/logging.h"


bool CefBrowserCToCpp::CanGoBack()
{
  if(CEF_MEMBER_MISSING(struct_, can_go_back))
    return false;

  return struct_->can_go_back(struct_) ? true : false;
}

void CefBrowserCToCpp::GoBack()
{
  if(CEF_MEMBER_MISSING(struct_, go_back))
    return;

  struct_->go_back(struct_);
}

bool CefBrowserCToCpp::CanGoForward()
{
  if(CEF_MEMBER_MISSING(struct_, can_go_forward))
    return false;
  
  return struct_->can_go_forward(struct_);
}

void CefBrowserCToCpp::GoForward()
{
  if(CEF_MEMBER_MISSING(struct_, go_forward))
    return;

  struct_->go_forward(struct_);
}

void CefBrowserCToCpp::Reload()
{
  if(CEF_MEMBER_MISSING(struct_, reload))
    return;

  struct_->reload(struct_);
}

void CefBrowserCToCpp::StopLoad()
{
  if(CEF_MEMBER_MISSING(struct_, stop_load))
    return;
  
  struct_->stop_load(struct_);
}

void CefBrowserCToCpp::Undo(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, undo))
    return;
  
  struct_->undo(struct_, targetFrame);
}

void CefBrowserCToCpp::Redo(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, redo))
    return;
  
  struct_->redo(struct_, targetFrame);
}

void CefBrowserCToCpp::Cut(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, cut))
    return;
  
  struct_->cut(struct_, targetFrame);
}

void CefBrowserCToCpp::Copy(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, copy))
    return;
  
  struct_->copy(struct_, targetFrame);
}

void CefBrowserCToCpp::Paste(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, paste))
    return;
  
  struct_->paste(struct_, targetFrame);
}

void CefBrowserCToCpp::Delete(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, del))
    return;
  
  struct_->del(struct_, targetFrame);
}

void CefBrowserCToCpp::SelectAll(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, select_all))
    return;
  
  struct_->select_all(struct_, targetFrame);
}

void CefBrowserCToCpp::Print(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, print))
    return;
  
  struct_->print(struct_, targetFrame);
}

void CefBrowserCToCpp::ViewSource(TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, view_source))
    return;
  
  struct_->view_source(struct_, targetFrame);
}

std::wstring CefBrowserCToCpp::GetSource(TargetFrame targetFrame)
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_source))
    return str;

  cef_string_t cef_str = struct_->get_source(struct_, targetFrame);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}

std::wstring CefBrowserCToCpp::GetText(TargetFrame targetFrame)
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_text))
    return str;

  cef_string_t cef_str = struct_->get_text(struct_, targetFrame);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}

void CefBrowserCToCpp::LoadRequest(CefRefPtr<CefRequest> request)
{
  if(CEF_MEMBER_MISSING(struct_, load_request))
    return;

  CefRequestCToCpp* rp = static_cast<CefRequestCToCpp*>(request.get());
  rp->UnderlyingAddRef();
  struct_->load_request(struct_, rp->GetStruct());
}

void CefBrowserCToCpp::LoadURL(const std::wstring& url,
                               const std::wstring& frame)
{
  if(CEF_MEMBER_MISSING(struct_, load_url))
    return;

  struct_->load_url(struct_, url.c_str(), frame.c_str());
}

void CefBrowserCToCpp::LoadString(const std::wstring& string,
                                  const std::wstring& url)
{
  if(CEF_MEMBER_MISSING(struct_, load_string))
    return;
  
  struct_->load_string(struct_, string.c_str(), url.c_str());
}

void CefBrowserCToCpp::LoadStream(CefRefPtr<CefStreamReader> stream,
                                  const std::wstring& url)
{
  if(CEF_MEMBER_MISSING(struct_, load_stream))
    return;

  CefStreamReaderCToCpp* sp =
      static_cast<CefStreamReaderCToCpp*>(stream.get());
  sp->UnderlyingAddRef();
  struct_->load_stream(struct_, sp->GetStruct(), url.c_str());
}

void CefBrowserCToCpp::ExecuteJavaScript(const std::wstring& js_code,
                                         const std::wstring& script_url,
                                         int start_line,
                                         TargetFrame targetFrame)
{
  if(CEF_MEMBER_MISSING(struct_, execute_javascript))
      return;

  struct_->execute_javascript(struct_, js_code.c_str(), script_url.c_str(),
      start_line, targetFrame);
}

bool CefBrowserCToCpp::AddJSHandler(const std::wstring& classname,
                                    CefRefPtr<CefJSHandler> handler)
{
  if(CEF_MEMBER_MISSING(struct_, add_jshandler))
    return false;

  CefJSHandlerCppToC* hp = new CefJSHandlerCppToC(handler);
  hp->AddRef();
  return struct_->add_jshandler(struct_, classname.c_str(), hp->GetStruct());
  return true;
}

bool CefBrowserCToCpp::HasJSHandler(const std::wstring& classname)
{
  if(CEF_MEMBER_MISSING(struct_, has_jshandler))
    return false;

  return struct_->has_jshandler(struct_, classname.c_str());
}

CefRefPtr<CefJSHandler> CefBrowserCToCpp::GetJSHandler(const std::wstring& classname)
{
  if(CEF_MEMBER_MISSING(struct_, get_jshandler))
    return NULL;

  CefJSHandlerCppToC::Struct* hp =
      reinterpret_cast<CefJSHandlerCppToC::Struct*>(
          struct_->get_jshandler(struct_, classname.c_str()));
  if(hp) {
    CefRefPtr<CefJSHandler> handlerPtr(hp->class_->GetClass());
    hp->class_->UnderlyingRelease();
    return handlerPtr;
  }

  return NULL;
}

bool CefBrowserCToCpp::RemoveJSHandler(const std::wstring& classname)
{
  if(CEF_MEMBER_MISSING(struct_, remove_jshandler))
      return false;
  
  return struct_->remove_jshandler(struct_, classname.c_str());
}

void CefBrowserCToCpp::RemoveAllJSHandlers()
{
  if(CEF_MEMBER_MISSING(struct_, remove_all_jshandlers))
      return;

  struct_->remove_all_jshandlers(struct_);
}

CefWindowHandle CefBrowserCToCpp::GetWindowHandle()
{
  if(CEF_MEMBER_MISSING(struct_, get_window_handle))
      return 0;
  
  return struct_->get_window_handle(struct_);
}

bool CefBrowserCToCpp::IsPopup()
{
  if(CEF_MEMBER_MISSING(struct_, is_popup))
    return false;
  
  return struct_->is_popup(struct_);
}

CefRefPtr<CefHandler> CefBrowserCToCpp::GetHandler()
{
  if(CEF_MEMBER_MISSING(struct_, get_handler))
    return NULL;
  
  CefHandlerCppToC::Struct* hp =
      reinterpret_cast<CefHandlerCppToC::Struct*>(
          struct_->get_handler(struct_));
  if(hp) {
    CefRefPtr<CefHandler> handlerPtr(hp->class_->GetClass());
    hp->class_->UnderlyingRelease();
    return handlerPtr;
  }

  return NULL;
}

std::wstring CefBrowserCToCpp::GetURL()
{
  std::wstring str;
  if(CEF_MEMBER_MISSING(struct_, get_url))
    return str;

  cef_string_t cef_str = struct_->get_url(struct_);
  if(cef_str) {
    str = cef_str;
    cef_string_free(cef_str);
  }
  return str;
}
