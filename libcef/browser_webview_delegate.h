// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
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
#include "base/scoped_ptr.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webview_delegate.h"
#if defined(OS_WIN)
#include "browser_drag_delegate.h"
#include "browser_drop_delegate.h"
#endif
#include "browser_navigation_controller.h"

class CefBrowserImpl;
struct ContextMenuMediaParams;
struct WebPreferences;
class GURL;
class WebWidgetHost;

class BrowserWebViewDelegate : public base::RefCounted<BrowserWebViewDelegate>,
                               public WebViewDelegate {
 public:
  BrowserWebViewDelegate(CefBrowserImpl* browser) 
    : policy_delegate_enabled_(false),
      policy_delegate_is_permissive_(false),
      browser_(browser),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1),
      smart_insert_delete_enabled_(true)
#if defined(OS_WIN)
      , select_trailing_whitespace_enabled_(true)
#else
      , select_trailing_whitespace_enabled_(false)
#endif
#if defined(OS_LINUX)
      , cursor_type_(GDK_X_CURSOR)
#endif
      { 
  }
  virtual ~BrowserWebViewDelegate() {}

  // WebViewDelegate
  virtual WebView* CreateWebView(WebView* webview,
                                 bool user_gesture,
                                 const GURL& creator_url);
  virtual WebKit::WebWidget* CreatePopupWidget(
    WebView* webview,
    bool activatable);
  virtual WebPluginDelegate* CreatePluginDelegate(
    WebView* webview,
    const GURL& url,
    const std::string& mime_type,
    const std::string& clsid,
    std::string* actual_mime_type);
  virtual WebKit::WebMediaPlayer* CreateWebMediaPlayer(
    WebKit::WebMediaPlayerClient* client);
  virtual WebKit::WebWorker* CreateWebWorker(WebKit::WebWorkerClient* client);
  virtual void OpenURL(WebView* webview,
                       const GURL& url,
                       const GURL& referrer,
                       WebKit::WebNavigationPolicy policy);
  virtual void DidMovePlugin(const WebPluginGeometry& move);
  virtual void RunJavaScriptAlert(WebFrame* webframe,
                                  const std::wstring& message);
  virtual bool RunJavaScriptConfirm(WebFrame* webframe,
                                    const std::wstring& message);
  virtual bool RunJavaScriptPrompt(WebFrame* webframe,
                                   const std::wstring& message,
                                   const std::wstring& default_value,
                                   std::wstring* result);
  virtual void SetStatusbarText(WebView* webview,
                                const std::wstring& message);
  virtual void AddMessageToConsole(WebView* webview,
                                   const std::wstring& message,
                                   unsigned int line_no,
                                   const std::wstring& source_id);
  virtual void StartDragging(WebView* webview,
                             const WebKit::WebDragData& drop_data);
  virtual void ShowContextMenu(WebView* webview,
                               ContextNodeType node_type,
                               int x,
                               int y,
                               const GURL& link_url,
                               const GURL& image_url,
                               const GURL& page_url,
                               const GURL& frame_url,
                               const ContextMenuMediaParams& media_params,
                               const std::wstring& selection_text,
                               const std::wstring& misspelled_word,
                               int edit_flags,
                               const std::string& security_info,
                               const std::string& frame_charset);
  virtual void DidCreateDataSource(WebFrame* frame,
                                   WebKit::WebDataSource* ds);
  virtual void DidStartProvisionalLoadForFrame(
    WebView* webview,
    WebFrame* frame,
    NavigationGesture gesture);
  virtual void DidReceiveServerRedirectForProvisionalLoadForFrame(
    WebView* webview, WebFrame* frame);
  virtual void DidFailProvisionalLoadWithError(
    WebView* webview,
    const WebKit::WebURLError& error,
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
                                    const WebKit::WebURLError& error,
                                    WebFrame* for_frame);

  virtual void AssignIdentifierToRequest(WebView* webview,
                                         uint32 identifier,
                                         const WebKit::WebURLRequest& request);
  virtual void WillSendRequest(WebView* webview,
                               uint32 identifier,
                               WebKit::WebURLRequest* request);
  virtual void DidFinishLoading(WebView* webview, uint32 identifier);
  virtual void DidFailLoadingWithError(WebView* webview,
                                       uint32 identifier,
                                       const WebKit::WebURLError& error);

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
  virtual WebKit::WebNavigationPolicy PolicyForNavigationAction(
    WebView* webview,
    WebFrame* frame,
    const WebKit::WebURLRequest& request,
    WebKit::WebNavigationType type,
    WebKit::WebNavigationPolicy default_policy,
    bool is_redirect);
  virtual void NavigateBackForwardSoon(int offset);
  virtual int GetHistoryBackListCount();
  virtual int GetHistoryForwardListCount();
  virtual void TakeFocus(WebView* webview, bool reverse);

  // WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect& rect);
  virtual void didScrollRect(int dx, int dy,
                             const WebKit::WebRect& clip_rect);
  virtual void didFocus();
  virtual void didBlur();
  virtual void didChangeCursor(const WebKit::WebCursorInfo& cursor);
  virtual void closeWidgetSoon();
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void runModal();
  virtual WebKit::WebRect windowRect();
  virtual void setWindowRect(const WebKit::WebRect& rect);
  virtual WebKit::WebRect rootWindowRect();
  virtual WebKit::WebRect windowResizerRect();
  virtual WebKit::WebScreenInfo screenInfo();

  void SetSmartInsertDeleteEnabled(bool enabled);
  void SetSelectTrailingWhitespaceEnabled(bool enabled);

  // Additional accessors
  WebFrame* top_loading_frame() { return top_loading_frame_; }
#if defined(OS_WIN)
  IDropTarget* drop_delegate() { return drop_delegate_.get(); }
  IDropSource* drag_delegate() { return drag_delegate_.get(); }
#endif

  void set_pending_extra_data(BrowserExtraData* extra_data) {
    pending_extra_data_.reset(extra_data);
  }

  // Methods for modifying WebPreferences
  void SetUserStyleSheetEnabled(bool is_enabled);
  void SetUserStyleSheetLocation(const GURL& location);

  // Sets the webview as a drop target.
  void RegisterDragDrop();

  void SetCustomPolicyDelegate(bool is_custom, bool is_permissive);
  void WaitForPolicyDelegate();

  CefBrowserImpl* GetBrowser() { return browser_; }

 protected:
  // Called when the URL of the page changes.
  void UpdateAddressBar(WebView* webView);

  // Default handling of JavaScript messages.
  void ShowJavaScriptAlert(WebFrame* webframe, const std::wstring& message);
  bool ShowJavaScriptConfirm(WebFrame* webframe, const std::wstring& message);
  bool ShowJavaScriptPrompt(WebFrame* webframe, const std::wstring& message,
    const std::wstring& default_value, std::wstring* result);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebFrame*);

  WebWidgetHost* GetWidgetHost();

  void UpdateForCommittedLoad(WebFrame* webframe, bool is_new_navigation);
  void UpdateURL(WebFrame* frame);
  void UpdateSessionHistory(WebFrame* frame);

  // Get a string suitable for dumping a frame to the console.
  std::wstring GetFrameDescription(WebFrame* webframe);

 private:
  // Causes navigation actions just printout the intended navigation instead 
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool policy_delegate_enabled_;

  // Toggles the behavior of the policy delegate.  If true, then navigations
  // will be allowed.  Otherwise, they will be ignored (dropped).
  bool policy_delegate_is_permissive_;

  // Non-owning pointer.  The delegate is owned by the host.
  CefBrowserImpl* browser_;

  // This is non-NULL IFF a load is in progress.
  WebFrame* top_loading_frame_;

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

  scoped_ptr<BrowserExtraData> pending_extra_data_;

  // true if we want to enable smart insert/delete.
  bool smart_insert_delete_enabled_;

  // true if we want to enable selection of trailing whitespaces
  bool select_trailing_whitespace_enabled_;

  WebCursor current_cursor_;
#if defined(OS_WIN)
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
  
  DISALLOW_COPY_AND_ASSIGN(BrowserWebViewDelegate);
};

#endif // _BROWSER_WEBVIEW_DELEGATE_H
