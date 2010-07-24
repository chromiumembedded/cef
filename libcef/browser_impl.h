// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_IMPL_H
#define _BROWSER_IMPL_H

#include "include/cef.h"

#include "webview_host.h"
#include "browser_webview_delegate.h"
#include "browser_navigation_controller.h"
#include "cef_thread.h"
#if defined(OS_WIN)
#include "printing/win_printing_context.h"
#endif

#include "third_party/WebKit/WebKit/chromium/public/WebFindOptions.h"

namespace base {
class WaitableEvent;
}
namespace WebKit {
class WebView;
}

#define BUFFER_SIZE   32768


// Implementation of CefBrowser.
class CefBrowserImpl : public CefThreadSafeBase<CefBrowser>
{
public:
  CefBrowserImpl(CefWindowInfo& windowInfo, bool popup,
                 CefRefPtr<CefHandler> handler);
  virtual ~CefBrowserImpl() {}

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
  virtual void SetFocus(bool enable);
  virtual CefWindowHandle GetWindowHandle();
  virtual bool IsPopup();
  virtual CefRefPtr<CefHandler> GetHandler();
  virtual CefRefPtr<CefFrame> GetMainFrame();
  virtual CefRefPtr<CefFrame> GetFocusedFrame();
  virtual CefRefPtr<CefFrame> GetFrame(const std::wstring& name);
  virtual void GetFrameNames(std::vector<std::wstring>& names);
  virtual void Find(int identifier, const std::wstring& searchText,
                    bool forward, bool matchCase, bool findNext);
  virtual void StopFinding(bool clearSelection);
  
  // CefFrames are light-weight objects managed by the browser and loosely
  // coupled to a WebFrame object by name.  If a CefFrame object does not
  // already exist for the specified WebFrame one will be created.  There is no
  // guarantee that the same CefFrame object will be returned across different
  // calls to this function.
  CefRefPtr<CefFrame> GetCefFrame(WebKit::WebFrame* frame);
  void RemoveCefFrame(const std::wstring& name);

  // Return the WebFrame object associated with the specified CefFrame.  This
  // may return NULL if no WebFrame with the CefFrame's name exists.
  WebKit::WebFrame* GetWebFrame(CefRefPtr<CefFrame> frame);

  // Frame-related methods
  void Undo(CefRefPtr<CefFrame> frame);
  void Redo(CefRefPtr<CefFrame> frame);
  void Cut(CefRefPtr<CefFrame> frame);
  void Copy(CefRefPtr<CefFrame> frame);
  void Paste(CefRefPtr<CefFrame> frame);
  void Delete(CefRefPtr<CefFrame> frame);
  void SelectAll(CefRefPtr<CefFrame> frame);
  void Print(CefRefPtr<CefFrame> frame);
  void ViewSource(CefRefPtr<CefFrame> frame);
  std::wstring GetSource(CefRefPtr<CefFrame> frame);
  std::wstring GetText(CefRefPtr<CefFrame> frame);
  void LoadRequest(CefRefPtr<CefFrame> frame,
                   CefRefPtr<CefRequest> request);
  void LoadURL(CefRefPtr<CefFrame> frame,
               const std::wstring& url);
  void LoadString(CefRefPtr<CefFrame> frame,
                  const std::wstring& string,
                  const std::wstring& url);
  void LoadStream(CefRefPtr<CefFrame> frame,
                  CefRefPtr<CefStreamReader> stream,
                  const std::wstring& url);
  void ExecuteJavaScript(CefRefPtr<CefFrame> frame,
                         const std::wstring& jsCode, 
                         const std::wstring& scriptUrl,
                         int startLine);
  std::wstring GetURL(CefRefPtr<CefFrame> frame);
  
  WebKit::WebView* GetWebView() const {
    return webviewhost_.get() ? webviewhost_->webview() : NULL;
  }
  WebViewHost* GetWebViewHost() const {
    return webviewhost_.get();
  }
  BrowserWebViewDelegate* GetWebViewDelegate() const {
    return delegate_.get();
  }
  HWND GetWebViewWndHandle() const {
    return webviewhost_->view_handle();
  }
  WebKit::WebWidget* GetPopup() const {
    return popuphost_ ? popuphost_->webwidget() : NULL;
  }
  WebWidgetHost* GetPopupHost() const {
    return popuphost_;
  }
  BrowserWebViewDelegate* GetPopupDelegate() const {
    return popup_delegate_.get();
  }
  HWND GetPopupWndHandle() const {
    return popuphost_->view_handle();
  }
  HWND GetMainWndHandle() const {
    return window_info_.m_hWnd;
  }


  ////////////////////////////////////////////////////////////
  // ALL UIT_* METHODS MUST ONLY BE CALLED ON THE UI THREAD //
  ////////////////////////////////////////////////////////////

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

  void UIT_CreateBrowser(const std::wstring& url);

  void UIT_LoadURL(CefFrame* frame,
                   const std::wstring& url);
  void UIT_LoadURLForRequest(CefFrame* frame,
                             const std::wstring& url,
                             const std::wstring& method,
                             const WebKit::WebHTTPBody& upload_data,
                             const CefRequest::HeaderMap& headers);
  void UIT_LoadURLForRequestRef(CefFrame* frame,
                                CefRequest* request);
  void UIT_LoadHTML(CefFrame* frame,
                    const std::wstring& html,
                    const std::wstring& url);
  void UIT_LoadHTMLForStreamRef(CefFrame* frame,
                                CefStreamReader* stream,
                                const std::wstring& url);
  void UIT_ExecuteJavaScript(CefFrame* frame,
                             const std::wstring& js_code, 
                             const std::wstring& script_url,
                             int start_line);
  void UIT_GoBackOrForward(int offset);
  void UIT_Reload();
  bool UIT_Navigate(const BrowserNavigationEntry& entry, bool reload);
  void UIT_SetFocus(WebWidgetHost* host, bool enable);

  CefRefPtr<CefBrowserImpl> UIT_CreatePopupWindow(const std::wstring& url);
  WebKit::WebWidget* UIT_CreatePopupWidget();
  void UIT_ClosePopupWidget();

  void UIT_Show(WebKit::WebNavigationPolicy policy);
  
  // Handles most simple browser actions
  void UIT_HandleActionView(CefHandler::MenuId menuId);
  void UIT_HandleAction(CefHandler::MenuId menuId, CefFrame* frame);

  // Save the document HTML to a temporary file and open in the default viewing
  // application
  bool UIT_ViewDocumentString(WebKit::WebFrame *frame);
  void UIT_GetDocumentStringNotify(CefFrame* frame,
                                   CefStreamWriter* writer,
                                   base::WaitableEvent* event);
  void UIT_GetDocumentTextNotify(CefFrame* frame, CefStreamWriter* writer,
                                 base::WaitableEvent* event);
  void UIT_CanGoBackNotify(bool *retVal, base::WaitableEvent* event);
  void UIT_CanGoForwardNotify(bool *retVal, base::WaitableEvent* event);

  bool UIT_CanGoBack() { return !nav_controller_->IsAtStart(); }
  bool UIT_CanGoForward() { return !nav_controller_->IsAtEnd(); }

  // Printing support
  void UIT_PrintPages(WebKit::WebFrame* frame);
  void UIT_PrintPage(int page_number, int total_pages,
                     const gfx::Size& canvas_size, WebKit::WebFrame* frame);
  int UIT_GetPagesCount(WebKit::WebFrame* frame);

  void UIT_SetUniqueID(int id) { unique_id_ = id; }
  int UIT_GetUniqueID() { return unique_id_; }

  void UIT_Find(int identifier, const std::wstring& search_text,
                const WebKit::WebFindOptions& options);
  void UIT_StopFinding(bool clear_selection);
  void UIT_NotifyFindStatus(int identifier, int count,
                            const WebKit::WebRect& selection_rect,
                            int active_match_ordinal, bool final_update);

  static bool ImplementsThreadSafeReferenceCounting() { return true; }

protected:
  CefWindowInfo window_info_;
  bool is_popup_;
  bool is_modal_;
  CefRefPtr<CefHandler> handler_;
  scoped_ptr<WebViewHost> webviewhost_;
  WebWidgetHost* popuphost_;
  scoped_ptr<BrowserWebViewDelegate> delegate_;
  scoped_ptr<BrowserWebViewDelegate> popup_delegate_;
  scoped_ptr<BrowserNavigationController> nav_controller_;

  std::wstring title_;

  // Context object used to manage printing.
  printing::PrintingContext print_context_;

  typedef std::map<std::wstring, CefFrame*> FrameMap;
  FrameMap frames_;
  CefFrame* frame_main_;

  // Unique browser ID assigned by the context.
  int unique_id_;
};


// Implementation of CefFrame.
class CefFrameImpl : public CefThreadSafeBase<CefFrame>
{
public:
  CefFrameImpl(CefBrowserImpl* browser, const std::wstring& name)
    : browser_(browser), name_(name) {}
  virtual ~CefFrameImpl() { browser_->RemoveCefFrame(name_); }

  // CefFrame methods
  virtual void Undo() { browser_->Undo(this); }
  virtual void Redo() { browser_->Redo(this); }
  virtual void Cut() { browser_->Cut(this); }
  virtual void Copy() { browser_->Copy(this); }
  virtual void Paste() { browser_->Paste(this); }
  virtual void Delete() { browser_->Delete(this); }
  virtual void SelectAll() { browser_->SelectAll(this); }
  virtual void Print() { browser_->Print(this); }
  virtual void ViewSource() { browser_->ViewSource(this); }
  virtual std::wstring GetSource() { return browser_->GetSource(this); }
  virtual std::wstring GetText() { return browser_->GetText(this); }
  virtual void LoadRequest(CefRefPtr<CefRequest> request)
    { return browser_->LoadRequest(this, request); }
  virtual void LoadURL(const std::wstring& url)
    { return browser_->LoadURL(this, url); }
  virtual void LoadString(const std::wstring& string,
                          const std::wstring& url)
    { return browser_->LoadString(this, string, url); }
  virtual void LoadStream(CefRefPtr<CefStreamReader> stream,
                          const std::wstring& url)
    { return browser_->LoadStream(this, stream, url); }
  virtual void ExecuteJavaScript(const std::wstring& jsCode, 
                                 const std::wstring& scriptUrl,
                                 int startLine) 
    { return browser_->ExecuteJavaScript(this, jsCode, scriptUrl, startLine); }
  virtual bool IsMain() { return name_.empty(); }
  virtual bool IsFocused();
  virtual std::wstring GetName() { return name_; }
  virtual std::wstring GetURL() { return browser_->GetURL(this); }

private:
  CefRefPtr<CefBrowserImpl> browser_;
  std::wstring name_;
};

#endif // _BROWSER_IMPL_H
