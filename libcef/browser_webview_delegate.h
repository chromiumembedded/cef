// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BrowserWebViewDelegate class: 
// This class implements the WebViewDelegate methods for the test shell.  One
// instance is owned by each CefBrowser.

#ifndef _BROWSER_WEBVIEW_DELEGATE_H
#define _BROWSER_WEBVIEW_DELEGATE_H


#include <map>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_ptr.h"
#include "build/build_config.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"
#include "browser_navigation_controller.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "external_popup_menu_mac.h"
#endif

#if defined(OS_WIN)
class BrowserDragDelegate;
class WebDropTarget;
#endif

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdkcursor.h>
#endif

class CefBrowserImpl;
class GURL;
class WebWidgetHost;
class FilePath;

class BrowserWebViewDelegate : public WebKit::WebViewClient,
    public WebKit::WebFrameClient,
    public webkit::npapi::WebPluginPageDelegate,
    public base::SupportsWeakPtr<BrowserWebViewDelegate> {
 public:
  // WebKit::WebViewClient
  virtual WebKit::WebView* createView(
      WebKit::WebFrame* creator, const WebKit::WebURLRequest& request,
      const WebKit::WebWindowFeatures& features, const WebKit::WebString& name);
  virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType popup_type);
  virtual WebKit::WebExternalPopupMenu* createExternalPopupMenu(
      const WebKit::WebPopupMenuInfo& info,
      WebKit::WebExternalPopupMenuClient* client);
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(
      unsigned quota);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
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
  virtual bool handleCurrentKeyboardEvent();
  virtual bool runFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value, WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual void showContextMenu(
      WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url);
  virtual void setToolTipText(
      const WebKit::WebString& text, WebKit::WebTextDirection hint);
  virtual void startDragging(
      const  WebKit::WebDragData& data,
       WebKit::WebDragOperationsMask mask,
      const  WebKit::WebImage& image,
      const  WebKit::WebPoint& image_offset);
  virtual bool acceptsLoadDrops() { return true; }
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();

  // WebKit::WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect& rect);
  virtual void didScrollRect(int dx, int dy,
                             const WebKit::WebRect& clip_rect);
  virtual void scheduleComposite();
  virtual void scheduleAnimation();
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
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
    WebKit::WebFrame* frame, WebKit::WebApplicationCacheHostClient* client);
  virtual void willClose(WebKit::WebFrame*);
  virtual void loadURLExternally(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationPolicy);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationType, const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy, bool isRedirect);
  virtual bool canHandleRequest(
      WebKit::WebFrame*, const WebKit::WebURLRequest&) { return true; }
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request);
  virtual void didCreateDataSource(
      WebKit::WebFrame*, WebKit::WebDataSource*);
  virtual void didStartProvisionalLoad(WebKit::WebFrame*);
  virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame*, bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame*);
  virtual void didReceiveTitle(
      WebKit::WebFrame*, const WebKit::WebString& title,
      WebKit::WebTextDirection direction);
  virtual void didFailLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&);
  virtual void didFinishLoad(WebKit::WebFrame*);
  virtual void didNavigateWithinPage(
      WebKit::WebFrame*, bool isNewNavigation);
  virtual void willSendRequest(
      WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void reportFindInPageMatchCount(
      int request_id, int count, bool final_update);
  virtual void reportFindInPageSelection(
      int request_id, int active_match_ordinal, const WebKit::WebRect& sel);
  virtual void openFileSystem(
      WebKit::WebFrame* frame,
      WebKit::WebFileSystem::Type type,
      long long size,
      bool create,
      WebKit::WebFileSystemCallbacks* callbacks);

  // webkit_glue::WebPluginPageDelegate
  virtual webkit::npapi::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type);
  virtual void CreatedPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle);
  virtual void DidMovePlugin(
      const webkit::npapi::WebPluginGeometry& move);
  virtual void DidStartLoadingForPlugin() {}
  virtual void DidStopLoadingForPlugin() {}
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval) {}
  virtual WebKit::WebCookieJar* GetCookieJar();

  BrowserWebViewDelegate(CefBrowserImpl* browser);
  ~BrowserWebViewDelegate();
  void Reset();

  void SetSmartInsertDeleteEnabled(bool enabled);
  void SetSelectTrailingWhitespaceEnabled(bool enabled);

  // Additional accessors
#if defined(OS_WIN)
  BrowserDragDelegate* drag_delegate() { return drag_delegate_.get(); }
  WebDropTarget* drop_target() { return drop_target_.get(); }
#endif

  void set_pending_extra_data(BrowserExtraData* extra_data) {
    pending_extra_data_.reset(extra_data);
  }

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

  void SetEditCommand(const std::string& name, const std::string& value) {
    edit_command_name_ = name;
    edit_command_value_ = value;
  }

  void ClearEditCommand() {
    edit_command_name_.clear();
    edit_command_value_.clear();
  }

  CefBrowserImpl* GetBrowser() { return browser_; }

#if defined(OS_MACOSX)
  // Called after the external popup menu has been dismissed.
  void ClosePopupMenu();
#endif

  // Called after dragging has finished.
  void EndDragging();

 protected:
  // Default handling of JavaScript messages.
  void ShowJavaScriptAlert(WebKit::WebFrame* webframe,
                           const CefString& message);
  bool ShowJavaScriptConfirm(WebKit::WebFrame* webframe,
                             const CefString& message);
  bool ShowJavaScriptPrompt(WebKit::WebFrame* webframe,
                            const CefString& message,
                            const CefString& default_value,
                            CefString* result);

  // Called to show the file chooser dialog.
  bool ShowFileChooser(std::vector<FilePath>& file_names, 
                       const bool multi_select, 
                       const WebKit::WebString& title, 
                       const FilePath& default_file);

  // Called to show status messages.
  void ShowStatus(const WebKit::WebString& text, CefHandler::StatusType type);

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

  // For tracking session history.  See RenderView.
  int page_id_;
  int last_page_id_updated_;

  scoped_ptr<BrowserExtraData> pending_extra_data_;

  WebCursor current_cursor_;
#if defined(OS_WIN)
  // Classes needed by drag and drop.
  scoped_refptr<BrowserDragDelegate> drag_delegate_;
  scoped_refptr<WebDropTarget> drop_target_;
#endif

#if defined(OS_LINUX)
  // The type of cursor the window is currently using.
  // Used for judging whether a new SetCursor call is actually changing the
  // cursor.
  GdkCursorType cursor_type_;
#endif

#if defined(OS_MACOSX)
  // The external popup menu for the currently showing select popup.
  scoped_ptr<ExternalPopupMenu> external_popup_menu_;
#endif

  // true if we want to enable smart insert/delete.
  bool smart_insert_delete_enabled_;

  // true if we want to enable selection of trailing whitespaces
  bool select_trailing_whitespace_enabled_;

  // true if we should block any redirects
  bool block_redirects_;

  // Edit command associated to the current keyboard event.
  std::string edit_command_name_;
  std::string edit_command_value_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWebViewDelegate);
};

#endif // _BROWSER_WEBVIEW_DELEGATE_H
