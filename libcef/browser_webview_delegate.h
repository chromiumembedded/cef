// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BrowserWebViewDelegate class: 
// This class implements the WebViewDelegate methods for the test shell.  One
// instance is owned by each CefBrowser.

#ifndef _BROWSER_WEBVIEW_DELEGATE_H
#define _BROWSER_WEBVIEW_DELEGATE_H

#include "build/build_config.h"

#include <map>

#if defined(OS_LINUX)
#include <gdk/gdkcursor.h>
#endif

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/webview_delegate.h"
#include "webkit/glue/webwidget_delegate.h"
#if defined(OS_WIN)
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#endif

class CefBrowserImpl;
struct WebPreferences;
class GURL;
class WebDataSource;
class WebWidgetHost;

class BrowserWebViewDelegate : public base::RefCounted<BrowserWebViewDelegate>,
                            public WebViewDelegate {
 public:
  BrowserWebViewDelegate(CefBrowserImpl* shell) 
    : is_custom_policy_delegate_(false),
      browser_(shell),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1)
#if defined(OS_WIN)
      , custom_cursor_(NULL)
#elif defined(OS_LINUX)
      , cursor_type_(GDK_X_CURSOR)
#endif
      { 
  }
  virtual ~BrowserWebViewDelegate();

  // WebViewDelegate
  virtual WebView* CreateWebView(WebView* webview, bool user_gesture);
  virtual WebWidget* CreatePopupWidget(WebView* webview, bool focus_on_show);
  virtual WebPluginDelegate* CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type);
  virtual void OpenURL(WebView* webview,
                       const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition);
  virtual void RunJavaScriptAlert(WebView* webview,
                                  const std::wstring& message);
  virtual bool RunJavaScriptConfirm(WebView* webview,
                                    const std::wstring& message);
  virtual bool RunJavaScriptPrompt(WebView* webview,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result);
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id);
  virtual void StartDragging(WebView* webview,
                             const WebDropData& drop_data);
  virtual void ShowContextMenu(WebView* webview,
                               ContextNode::Type type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info);
  virtual void DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture);
  virtual void DidReceiveServerRedirectForProvisionalLoadForFrame(
    WebView* webview, WebFrame* frame);
  virtual void DidFailProvisionalLoadWithError(WebView* webview,
                                               const WebError& error,
                                               WebFrame* frame);
  virtual void DidCommitLoadForFrame(WebView* webview, WebFrame* frame,
                                     bool is_new_navigation);
  virtual void DidReceiveTitle(WebView* webview,
                               const std::wstring& title,
                               WebFrame* frame);
  virtual void DidFinishDocumentLoadForFrame(WebView* webview,
                                             WebFrame* frame);
  virtual void DidHandleOnloadEventsForFrame(WebView* webview,
                                             WebFrame* frame);
  virtual void DidChangeLocationWithinPageForFrame(WebView* webview,
                                                   WebFrame* frame,
                                                   bool is_new_navigation);
  virtual void DidReceiveIconForFrame(WebView* webview, WebFrame* frame);

  virtual void WillPerformClientRedirect(WebView* webview,
                                         WebFrame* frame,
                                         const std::wstring& dest_url,
                                         unsigned int delay_seconds,
                                         unsigned int fire_date);
  virtual void DidCancelClientRedirect(WebView* webview, 
                                       WebFrame* frame);

  virtual void DidFinishLoadForFrame(WebView* webview, WebFrame* frame);
  virtual void DidFailLoadWithError(WebView* webview,
                                    const WebError& error,
                                    WebFrame* forFrame);

  virtual void AssignIdentifierToRequest(WebView* webview,
                                         uint32 identifier,
                                         const WebRequest& request);
  virtual void WillSendRequest(WebView* webview,
                               uint32 identifier,
                               WebRequest* request);
  virtual void DidFinishLoading(WebView* webview, uint32 identifier);
  virtual void DidFailLoadingWithError(WebView* webview,
                                       uint32 identifier,
                                       const WebError& error);

  virtual bool ShouldBeginEditing(WebView* webview, std::wstring range);
  virtual bool ShouldEndEditing(WebView* webview, std::wstring range);
  virtual bool ShouldInsertNode(WebView* webview, 
                                std::wstring node, 
                                std::wstring range,
                                std::wstring action);
  virtual bool ShouldInsertText(WebView* webview, 
                                std::wstring text, 
                                std::wstring range,
                                std::wstring action);
  virtual bool ShouldChangeSelectedRange(WebView* webview, 
                                         std::wstring fromRange, 
                                         std::wstring toRange, 
                                         std::wstring affinity, 
                                         bool stillSelecting);
  virtual bool ShouldDeleteRange(WebView* webview, std::wstring range);
  virtual bool ShouldApplyStyle(WebView* webview, 
                                std::wstring style,
                                std::wstring range);
  virtual bool SmartInsertDeleteEnabled();
  virtual void DidBeginEditing();
  virtual void DidChangeSelection();
  virtual void DidChangeContents();
  virtual void DidEndEditing();

  virtual void DidStartLoading(WebView* webview);
  virtual void DidStopLoading(WebView* webview);

  virtual void WindowObjectCleared(WebFrame* webframe);
  virtual WindowOpenDisposition DispositionForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebRequest* request,
    WebNavigationType type,
    WindowOpenDisposition disposition,
    bool is_redirect);
  void SetCustomPolicyDelegate(bool isCustom);
  virtual WebHistoryItem* GetHistoryEntryAtOffset(int offset);
  virtual int GetHistoryBackListCount();
  virtual int GetHistoryForwardListCount();

  // WebWidgetDelegate
  virtual gfx::ViewHandle GetContainingWindow(WebWidget* webwidget);
  virtual void DidInvalidateRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void DidScrollRect(WebWidget* webwidget, int dx, int dy,
                             const gfx::Rect& clip_rect);
  virtual void Show(WebWidget* webview, WindowOpenDisposition disposition);
  virtual void CloseWidgetSoon(WebWidget* webwidget);
  virtual void Focus(WebWidget* webwidget);
  virtual void Blur(WebWidget* webwidget);
  virtual void SetCursor(WebWidget* webwidget, 
                         const WebCursor& cursor);
  virtual void GetWindowRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void SetWindowRect(WebWidget* webwidget, const gfx::Rect& rect);
  virtual void GetRootWindowRect(WebWidget *,gfx::Rect *);
  virtual void GetRootWindowResizerRect(WebWidget* webwidget, gfx::Rect* rect);
  virtual void DidMove(WebWidget* webwidget, const WebPluginGeometry& move);
  virtual void RunModal(WebWidget* webwidget);
  virtual bool IsHidden();
  virtual void AddRef() {
    base::RefCounted<BrowserWebViewDelegate>::AddRef();
  }
  virtual void Release() {
    base::RefCounted<BrowserWebViewDelegate>::Release();
  }

  // Additional accessors
  WebFrame* top_loading_frame() { return top_loading_frame_; }
#if defined(OS_WIN)
  IDropTarget* drop_delegate() { return drop_delegate_.get(); }
  IDropSource* drag_delegate() { return drag_delegate_.get(); }
#endif
  
  // Methods for modifying WebPreferences
  void SetUserStyleSheetEnabled(bool is_enabled);
  void SetUserStyleSheetLocation(const GURL& location);

  // Sets the webview as a drop target.
  void RegisterDragDrop();

  CefBrowserImpl* GetBrowser() { return browser_; }

 protected:
  // Called when the URL of the page changes.
  void UpdateAddressBar(WebView* webView);

  // Default handling of JavaScript messages.
  void ShowJavaScriptAlert(WebView* webview, const std::wstring& message);
  bool ShowJavaScriptConfirm(WebView* webview, const std::wstring& message);
  bool ShowJavaScriptPrompt(WebView* webview, const std::wstring& message,
    const std::wstring& default_value, std::wstring* result);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebDataSource* data_source);

  WebWidgetHost* GetHostForWidget(WebWidget* webwidget);

  void UpdateForCommittedLoad(WebFrame* webframe, bool is_new_navigation);
  void UpdateURL(WebFrame* frame);
  void UpdateSessionHistory(WebFrame* frame);

  // Get a string suitable for dumping a frame to the console.
  std::wstring GetFrameDescription(WebFrame* webframe);

 private:
  // Causes navigation actions just printout the intended navigation instead 
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool is_custom_policy_delegate_;

  // Non-owning pointer.  The delegate is owned by the host.
  CefBrowserImpl* browser_;

  // This is non-NULL IFF a load is in progress.
  WebFrame* top_loading_frame_;

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

#if defined(OS_WIN)
  HCURSOR custom_cursor_;

  // Classes needed by drag and drop.
  scoped_refptr<BrowserDragDelegate> drag_delegate_;
  scoped_refptr<BrowserDropDelegate> drop_delegate_;
#endif

#if defined(OS_LINUX)
  // The type of cursor the window is currently using.
  // Used for judging whether a new SetCursor call is actually changing the
  // cursor.
  GdkCursorType cursor_type_;
#endif
  
  DISALLOW_EVIL_CONSTRUCTORS(BrowserWebViewDelegate);
};

#endif // _BROWSER_WEBVIEW_DELEGATE_H
