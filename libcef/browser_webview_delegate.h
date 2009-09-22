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
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "webkit/api/public/WebFrameClient.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webplugin_page_delegate.h"
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

class BrowserWebViewDelegate : public WebViewDelegate,
    public WebKit::WebFrameClient,
    public webkit_glue::WebPluginPageDelegate,
    public base::SupportsWeakPtr<BrowserWebViewDelegate> {
 public:
  // WebViewDelegate
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
  
  // WebKit::WebViewClient
  virtual WebView* createView(WebKit::WebFrame* creator);
  virtual WebKit::WebWidget* createPopupMenu(bool activatable);
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual bool shouldBeginEditing(const WebKit::WebRange& range);
  virtual bool shouldEndEditing(const WebKit::WebRange& range);
  virtual bool shouldInsertNode(
      const WebKit::WebNode& node, const WebKit::WebRange& range,
      WebKit::WebEditingAction action);
  virtual bool shouldInsertText(
      const WebKit::WebString& text, const WebKit::WebRange& range,
      WebKit::WebEditingAction action);
  virtual bool shouldChangeSelectedRange(
      const WebKit::WebRange& from, const WebKit::WebRange& to,
      WebKit::WebTextAffinity affinity, bool still_selecting);
  virtual bool shouldDeleteRange(const WebKit::WebRange& range);
  virtual bool shouldApplyStyle(
      const WebKit::WebString& style, const WebKit::WebRange& range);
  virtual bool isSmartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual void setInputMethodEnabled(bool enabled) {}
  virtual void didBeginEditing();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didChangeContents();
  virtual void didExecuteCommand(const WebKit::WebString& command_name);
  virtual void didEndEditing();
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value, WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setToolTipText(
      const WebKit::WebString& text, WebKit::WebTextDirection hint);
  virtual void startDragging(
      const WebKit::WebPoint& from, const WebKit::WebDragData& data,
      WebKit::WebDragOperationsMask mask);
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void didAddHistoryItem();
  
  // WebKit::WebWidgetClient
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

  // WebKit::WebFrameClient
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame*, const WebKit::WebPluginParams&);
  virtual WebKit::WebWorker* createWorker(
      WebKit::WebFrame*, WebKit::WebWorkerClient*);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame*, WebKit::WebMediaPlayerClient*);
  virtual void willClose(WebKit::WebFrame*);
  virtual void loadURLExternally(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationPolicy);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationType, WebKit::WebNavigationPolicy default_policy,
      bool isRedirect);
  virtual void willSubmitForm(WebKit::WebFrame*, const WebKit::WebForm&);
  virtual void willPerformClientRedirect(
      WebKit::WebFrame*, const WebKit::WebURL& from, const WebKit::WebURL& to,
      double interval, double fire_time);
  virtual void didCancelClientRedirect(WebKit::WebFrame*);
  virtual void didCompleteClientRedirect(
      WebKit::WebFrame*, const WebKit::WebURL& from);
  virtual void didCreateDataSource(
      WebKit::WebFrame*, WebKit::WebDataSource*);
  virtual void didStartProvisionalLoad(WebKit::WebFrame*);
  virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didReceiveDocumentData(
      WebKit::WebFrame*, const char* data, size_t length,
      bool& preventDefault);
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame*, bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame*);
  virtual void didCreateDocumentElement(WebKit::WebFrame*);
  virtual void didReceiveTitle(
      WebKit::WebFrame*, const WebKit::WebString& title);
  virtual void didFinishDocumentLoad(WebKit::WebFrame*);
  virtual void didHandleOnloadEvents(WebKit::WebFrame*);
  virtual void didFailLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didFinishLoad(WebKit::WebFrame*);
  virtual void didChangeLocationWithinPage(
      WebKit::WebFrame*, bool isNewNavigation);
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame*) {}
  virtual void assignIdentifierToRequest(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLRequest&);
  virtual void willSendRequest(
      WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void didReceiveResponse(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLResponse&);
  virtual void didFinishResourceLoad(
      WebKit::WebFrame*, unsigned identifier);
  virtual void didFailResourceLoad(
      WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLError&);
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      const WebKit::WebURLResponse&);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin);
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame*);
  virtual void didChangeContentsSize(
      WebKit::WebFrame*, const WebKit::WebSize&);

  // webkit_glue::WebPluginPageDelegate
  virtual webkit_glue::WebPluginDelegate* CreatePluginDelegate(
      const GURL& url,
      const std::string& mime_type,
      std::string* actual_mime_type);
  virtual void CreatedPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void DidMovePlugin(
      const webkit_glue::WebPluginGeometry& move);
  virtual void DidStartLoadingForPlugin() {}
  virtual void DidStopLoadingForPlugin() {}
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval) {}

  BrowserWebViewDelegate(CefBrowserImpl* browser);
  ~BrowserWebViewDelegate();
  void Reset();

  void SetSmartInsertDeleteEnabled(bool enabled);
  void SetSelectTrailingWhitespaceEnabled(bool enabled);

  // Additional accessors
  WebKit::WebFrame* top_loading_frame() { return top_loading_frame_; }
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
  void RevokeDragDrop();

  void ResetDragDrop();

  void SetCustomPolicyDelegate(bool is_custom, bool is_permissive);
  void WaitForPolicyDelegate();

  void set_block_redirects(bool block_redirects) {
    block_redirects_ = block_redirects;
  }
  bool block_redirects() const {
    return block_redirects_;
  }

  CefBrowserImpl* GetBrowser() { return browser_; }

 protected:
  // Called when the URL of the page changes.
  void UpdateAddressBar(WebView* webView);

  // Default handling of JavaScript messages.
  void ShowJavaScriptAlert(WebKit::WebFrame* webframe,
                           const std::wstring& message);
  bool ShowJavaScriptConfirm(WebKit::WebFrame* webframe,
                             const std::wstring& message);
  bool ShowJavaScriptPrompt(WebKit::WebFrame* webframe,
                            const std::wstring& message,
                            const std::wstring& default_value,
                            std::wstring* result);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebKit::WebFrame*);

  WebWidgetHost* GetWidgetHost();

  void UpdateForCommittedLoad(WebKit::WebFrame* webframe,
                              bool is_new_navigation);
  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateSessionHistory(WebKit::WebFrame* frame);

 private:
  // Causes navigation actions just printout the intended navigation instead 
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool policy_delegate_enabled_;

  // Toggles the behavior of the policy delegate.  If true, then navigations
  // will be allowed.  Otherwise, they will be ignored (dropped).
  bool policy_delegate_is_permissive_;

  // If true, the policy delegate will signal layout test completion.
  bool policy_delegate_should_notify_done_;

  // Non-owning pointer.  The delegate is owned by the host.
  CefBrowserImpl* browser_;

  // This is non-NULL IFF a load is in progress.
  WebKit::WebFrame* top_loading_frame_;

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

  scoped_ptr<BrowserExtraData> pending_extra_data_;

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
  
  // true if we want to enable smart insert/delete.
  bool smart_insert_delete_enabled_;

  // true if we want to enable selection of trailing whitespaces
  bool select_trailing_whitespace_enabled_;

  // true if we should block any redirects
  bool block_redirects_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWebViewDelegate);
};

#endif // _BROWSER_WEBVIEW_DELEGATE_H
