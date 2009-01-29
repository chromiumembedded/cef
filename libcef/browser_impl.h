// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_IMPL_H
#define _BROWSER_IMPL_H

#include "../include/cef.h"
#include "jscontainer.h"
#include "context.h"

#include "webview_host.h"
#include "browser_webview_delegate.h"
#include "browser_navigation_controller.h"
#if defined(OS_WIN)
#include "printing/win_printing_context.h"
#endif

#include "webkit/glue/webview.h"


// Implementation of CefBrowser.
class CefBrowserImpl : public CefThreadSafeBase<CefBrowser>
{
public:
  CefBrowserImpl(CefWindowInfo& windowInfo, bool popup,
                 CefRefPtr<CefHandler> handler, const std::wstring& url);
  virtual ~CefBrowserImpl();

#if defined(OS_WIN)
  static LPCTSTR GetWndClass();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam,
                                  LPARAM lParam);
#endif

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

  void SetURL(const std::wstring& url);

  ////////////////////////////////////////////////////////////
  // ALL UIT_* METHODS MUST ONLY BE CALLED ON THE UI THREAD //
  ////////////////////////////////////////////////////////////

  WebView* UIT_GetWebView() const {
    REQUIRE_UIT();
    return webviewhost_.get() ? webviewhost_->webview() : NULL;
  }
  WebViewHost* UIT_GetWebViewHost() const {
    REQUIRE_UIT();
    return webviewhost_.get();
  }
  HWND UIT_GetWebViewWndHandle() const {
    REQUIRE_UIT();
    return webviewhost_->window_handle();
  }
  WebWidget* UIT_GetPopup() const {
    REQUIRE_UIT();
    return popuphost_ ? popuphost_->webwidget() : NULL;
  }
  WebWidgetHost* UIT_GetPopupHost() const {
    REQUIRE_UIT();
    return popuphost_;
  }
  HWND UIT_GetPopupWndHandle() const {
    REQUIRE_UIT();
    return popuphost_->window_handle();
  }
  HWND UIT_GetMainWndHandle() const {
    REQUIRE_UIT();
    return window_info_.m_hWnd;
  }
  BrowserNavigationController* UIT_GetNavigationController() {
    REQUIRE_UIT();
    return nav_controller_.get();
  }

  // Return true to allow user editing such as entering text in form elements
  bool UIT_AllowEditing() { return true; }

  bool UIT_IsModal() {
    REQUIRE_UIT();
    return is_modal_;
  }
  void UIT_SetIsModal(bool val) {
    REQUIRE_UIT();
    is_modal_ = val;
  }

  void UIT_SetTitle(const std::wstring& title) {
    REQUIRE_UIT();
    title_ = title;
  }
  std::wstring UIT_GetTitle() {
    REQUIRE_UIT();
    return title_;
  }

  // UIThread functions must be executed from the UI thread.
  void UIT_CreateBrowser();
  void UIT_LoadURL(const std::wstring& url);
  void UIT_LoadURLForFrame(const std::wstring& url,
                                const std::wstring& frame_name);
  void UIT_LoadURLForRequest(const std::wstring& url,
                                  const std::wstring& frame_name,
                                  const std::wstring& method,
                                  net::UploadData *upload_data,
                                  const WebRequest::HeaderMap& headers);
  void UIT_LoadURLForRequestRef(CefRequest* request);
  void UIT_LoadHTML(const std::wstring& html,
                         const std::wstring& url);
  void UIT_LoadHTMLForStreamRef(CefStreamReader* stream,
                                     const std::wstring& url);
  void UIT_ExecuteJavaScript(const std::wstring& js_code, 
                             const std::wstring& script_url,
                             int start_line, TargetFrame targetFrame);
  void UIT_GoBackOrForward(int offset);
  void UIT_Reload();
  bool UIT_Navigate(const BrowserNavigationEntry& entry, bool reload);
  void UIT_SetFocus(WebWidgetHost* host, bool enable);

  // Called by the WebView delegate WindowObjectCleared() method, this
  // binds the C++ controller classes to window JavaScript objects.
  void UIT_BindJSObjectsToWindow(WebFrame* frame);

  CefRefPtr<CefBrowserImpl> UIT_CreatePopupWindow(const std::wstring& url);
  WebWidget* UIT_CreatePopupWidget(WebView* webview);
  void UIT_ClosePopupWidget();

  void UIT_Show(WebView* webview, WindowOpenDisposition disposition);
  
  // Handles most simple browser actions
  void UIT_HandleAction(CefHandler::MenuId menuId, TargetFrame target);

  // Save the document HTML to a temporary file and open in the default viewing
  // application
  bool UIT_ViewDocumentString(WebFrame *frame);
#if defined(OS_WIN)
  void UIT_GetDocumentStringNotify(TargetFrame targetFrame,
                                  CefStreamWriter* writer, HANDLE hEvent);
  void UIT_GetDocumentTextNotify(TargetFrame targetFrame,
                                CefStreamWriter* writer, HANDLE hEvent);
  void UIT_CanGoBackNotify(bool *retVal, HANDLE hEvent);
  void UIT_CanGoForwardNotify(bool *retVal, HANDLE hEvent);
#endif

  bool UIT_CanGoBack() { return !nav_controller_->IsAtStart(); }
  bool UIT_CanGoForward() { return !nav_controller_->IsAtEnd(); }

  // Printing support
  void UIT_PrintPages(WebFrame* frame);
  void UIT_PrintPage(int page_number, WebFrame* frame, int total_pages);
  void UIT_SwitchFrameToDisplayMediaType(WebFrame* frame);
  int UIT_SwitchFrameToPrintMediaType(WebFrame* frame);
  int UIT_GetPagesCount(WebFrame* frame);

#if defined(OS_WIN)
  void UIT_DisableWebView(bool val);
	bool UIT_IsWebViewDisabled() { return (webview_bitmap_ != NULL); }

  void UIT_CaptureWebViewBitmap(HBITMAP &bitmap, SIZE &size);
  void UIT_SetWebViewBitmap(HBITMAP bitmap, SIZE size);
	void UIT_GetWebViewBitmap(HBITMAP &bitmap, SIZE &size)
	{ 
		bitmap = webview_bitmap_;
		size.cx = webview_bitmap_size_.cx;
		size.cy = webview_bitmap_size_.cy;
	}
#endif

protected:
  CefWindowInfo window_info_;
  bool is_popup_;
  bool is_modal_;
  std::wstring url_;
  CefRefPtr<CefHandler> handler_;
  scoped_ptr<WebViewHost> webviewhost_;
  WebWidgetHost* popuphost_;
  scoped_refptr<BrowserWebViewDelegate> delegate_;
  scoped_ptr<BrowserNavigationController> nav_controller_;

  std::wstring title_;

  // Backup the view size before printing since it needs to be overriden. This
	// value is set to restore the view size when printing is done.
	gfx::Size printing_view_size_;
  // Context object used to manage printing.
	printing::PrintingContext print_context_;

  typedef std::map<std::wstring, CefRefPtr<CefJSContainer> > JSContainerMap;
  JSContainerMap jscontainers_;

#if defined(OS_WIN)
  HBITMAP webview_bitmap_;
	SIZE webview_bitmap_size_;
#endif
};

#endif // _BROWSER_IMPL_H
