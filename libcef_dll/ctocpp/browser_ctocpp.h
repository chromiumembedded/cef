// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _BROWSER_CTOCPP_H
#define _BROWSER_CTOCPP_H

#ifndef USING_CEF_SHARED
#pragma message("Warning: "__FILE__" may be accessed wrapper-side only")
#else // USING_CEF_SHARED

#include "cef.h"
#include "cef_capi.h"
#include "ctocpp.h"


// Wrap a C browser structure with a C++ browser class.
// This class may be instantiated and accessed wrapper-side only.
class CefBrowserCToCpp : public CefCToCpp<CefBrowser, cef_browser_t>
{
public:
  CefBrowserCToCpp(cef_browser_t* str)
    : CefCToCpp<CefBrowser, cef_browser_t>(str) {}
  virtual ~CefBrowserCToCpp() {}

  // CefBrowser methods
  virtual bool CanGoBack();
  virtual void GoBack();
  virtual bool CanGoForward();
  virtual void GoForward();
  virtual void Reload();
  virtual void StopLoad();
  virtual void Undo(TargetFrame targetFrame);
  virtual void Redo(TargetFrame targetFrame);
  virtual void Cut(TargetFrame targetFrame);
  virtual void Copy(TargetFrame targetFrame);
  virtual void Paste(TargetFrame targetFrame);
  virtual void Delete(TargetFrame targetFrame);
  virtual void SelectAll(TargetFrame targetFrame);
  virtual void SetFocus(bool enable);
  virtual void Print(TargetFrame targetFrame);
  virtual void ViewSource(TargetFrame targetFrame);
  virtual std::wstring GetSource(TargetFrame targetFrame);
  virtual std::wstring GetText(TargetFrame targetFrame);
  virtual void LoadRequest(CefRefPtr<CefRequest> request);
  virtual void LoadURL(const std::wstring& url, const std::wstring& frame);
  virtual void LoadString(const std::wstring& string,
                          const std::wstring& url);
  virtual void LoadStream(CefRefPtr<CefStreamReader> stream,
                          const std::wstring& url);
  virtual void ExecuteJavaScript(const std::wstring& js_code, 
                                 const std::wstring& script_url,
                                 int start_line, TargetFrame targetFrame);
  virtual bool AddJSHandler(const std::wstring& classname,
                            CefRefPtr<CefJSHandler> handler);
  virtual bool HasJSHandler(const std::wstring& classname);
  virtual CefRefPtr<CefJSHandler> GetJSHandler(const std::wstring& classname);
  virtual bool RemoveJSHandler(const std::wstring& classname);
  virtual void RemoveAllJSHandlers();
  virtual CefWindowHandle GetWindowHandle();
  virtual bool IsPopup();
  virtual CefRefPtr<CefHandler> GetHandler();
  virtual std::wstring GetURL();
};


#endif // USING_CEF_SHARED
#endif // _BROWSER_CTOCPP_H
