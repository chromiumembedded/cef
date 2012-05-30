// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BrowserWebViewDelegate class:
// This class implements the WebViewDelegate methods for the test shell.  One
// instance is owned by each CefBrowser.

#ifndef CEF_LIBCEF_BROWSER_WEBVIEW_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_WEBVIEW_DELEGATE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "libcef/browser_navigation_controller.h"
#include "libcef/browser_webcookiejar_impl.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileChooserParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPermissionClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "webkit/glue/webcursor.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"

#if defined(TOOLKIT_USES_GTK)
#include <gdk/gdk.h>  // NOLINT(build/include_order)
#endif

#if defined(OS_MACOSX)
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenuInfo.h"
#include "libcef/external_popup_menu_mac.h"
#endif

#if defined(OS_WIN)
class BrowserDragDelegate;
class WebDropTarget;
#endif

#if defined(OS_LINUX)
class WebDragSource;
class WebDropTarget;
#endif

class CefBrowserImpl;
class GURL;
class WebWidgetHost;
class FilePath;

class BrowserWebViewDelegate : public WebKit::WebViewClient,
    public WebKit::WebFrameClient,
    public WebKit::WebPermissionClient,
    public webkit::npapi::WebPluginPageDelegate,
    public base::SupportsWeakPtr<BrowserWebViewDelegate> {
 public:
  // WebKit::WebViewClient
  virtual WebKit::WebView* createView(
      WebKit::WebFrame* creator, const WebKit::WebURLRequest& request,
      const WebKit::WebWindowFeatures& features, const WebKit::WebString& name,
      WebKit::WebNavigationPolicy policy) OVERRIDE;
  virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType popup_type)
      OVERRIDE;
  virtual WebKit::WebExternalPopupMenu* createExternalPopupMenu(
      const WebKit::WebPopupMenuInfo& info,
      WebKit::WebExternalPopupMenuClient* client) OVERRIDE;
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(
      unsigned quota) OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* createGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes) OVERRIDE;
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line) OVERRIDE;
  virtual void printPage(WebKit::WebFrame* frame) OVERRIDE;
  virtual bool shouldBeginEditing(const WebKit::WebRange& range) OVERRIDE;
  virtual bool shouldEndEditing(const WebKit::WebRange& range) OVERRIDE;
  virtual bool shouldInsertNode(
      const WebKit::WebNode& node, const WebKit::WebRange& range,
      WebKit::WebEditingAction action) OVERRIDE;
  virtual bool shouldInsertText(
      const WebKit::WebString& text, const WebKit::WebRange& range,
      WebKit::WebEditingAction action) OVERRIDE;
  virtual bool shouldChangeSelectedRange(
      const WebKit::WebRange& from, const WebKit::WebRange& to,
      WebKit::WebTextAffinity affinity, bool still_selecting) OVERRIDE;
  virtual bool shouldDeleteRange(const WebKit::WebRange& range) OVERRIDE;
  virtual bool shouldApplyStyle(
      const WebKit::WebString& style, const WebKit::WebRange& range) OVERRIDE;
  virtual bool isSmartInsertDeleteEnabled() OVERRIDE;
  virtual bool isSelectTrailingWhitespaceEnabled() OVERRIDE;
  virtual bool handleCurrentKeyboardEvent() OVERRIDE;
  virtual bool runFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion) OVERRIDE;
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) OVERRIDE;
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) OVERRIDE;
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value, WebKit::WebString* actual_value)
      OVERRIDE;
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message) OVERRIDE;
  virtual void showContextMenu(
      WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data) OVERRIDE;
  virtual void setStatusText(const WebKit::WebString& text) OVERRIDE;
  virtual void setMouseOverURL(const WebKit::WebURL& url) OVERRIDE;
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url) OVERRIDE;
  virtual void setToolTipText(
      const WebKit::WebString& text, WebKit::WebTextDirection hint) OVERRIDE;
  virtual void startDragging(
      const  WebKit::WebDragData& data,
       WebKit::WebDragOperationsMask mask,
      const  WebKit::WebImage& image,
      const  WebKit::WebPoint& image_offset) OVERRIDE;
  virtual bool acceptsLoadDrops() OVERRIDE;
  virtual void focusNext() OVERRIDE;
  virtual void focusPrevious() OVERRIDE;
  virtual void focusedNodeChanged(const WebKit::WebNode& node) OVERRIDE;
  virtual void navigateBackForwardSoon(int offset) OVERRIDE;
  virtual int historyBackListCount() OVERRIDE;
  virtual int historyForwardListCount() OVERRIDE;

  // WebKit::WebWidgetClient
  virtual void didInvalidateRect(const WebKit::WebRect& rect) OVERRIDE;
  virtual void didScrollRect(int dx, int dy,
                             const WebKit::WebRect& clip_rect) OVERRIDE;
  virtual void scheduleComposite() OVERRIDE;
  virtual void scheduleAnimation() OVERRIDE;
  virtual void didFocus() OVERRIDE;
  virtual void didBlur() OVERRIDE;
  virtual void didChangeCursor(const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual void closeWidgetSoon() OVERRIDE;
  virtual void show(WebKit::WebNavigationPolicy policy) OVERRIDE;
  virtual void runModal() OVERRIDE;
  virtual WebKit::WebRect windowRect() OVERRIDE;
  virtual void setWindowRect(const WebKit::WebRect& rect) OVERRIDE;
  virtual WebKit::WebRect rootWindowRect() OVERRIDE;
  virtual WebKit::WebRect windowResizerRect() OVERRIDE;
  virtual WebKit::WebScreenInfo screenInfo() OVERRIDE;

  // WebKit::WebFrameClient
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame*, const WebKit::WebPluginParams&) OVERRIDE;
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame*, WebKit::WebMediaPlayerClient*) OVERRIDE;
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
    WebKit::WebFrame* frame, WebKit::WebApplicationCacheHostClient* client)
    OVERRIDE;
  virtual WebKit::WebCookieJar* cookieJar(WebKit::WebFrame*) OVERRIDE;
  virtual void willClose(WebKit::WebFrame*) OVERRIDE;
  virtual void loadURLExternally(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationPolicy) OVERRIDE;
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame*, const WebKit::WebURLRequest&,
      WebKit::WebNavigationType, const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy, bool isRedirect) OVERRIDE;
  virtual bool canHandleRequest(
      WebKit::WebFrame*, const WebKit::WebURLRequest&) OVERRIDE { return true; }
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request) OVERRIDE;
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request) OVERRIDE;
  virtual void didCreateDataSource(
      WebKit::WebFrame*, WebKit::WebDataSource*) OVERRIDE;
  virtual void didStartProvisionalLoad(WebKit::WebFrame*) OVERRIDE;
  virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*)
      OVERRIDE;
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&) OVERRIDE;
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame*, bool is_new_navigation) OVERRIDE;
  virtual void didCreateScriptContext(
      WebKit::WebFrame*, v8::Handle<v8::Context>, int extensionGroup,
      int worldId) OVERRIDE;
  virtual void willReleaseScriptContext(
      WebKit::WebFrame*, v8::Handle<v8::Context>, int worldId) OVERRIDE;
  virtual void didReceiveTitle(
      WebKit::WebFrame*, const WebKit::WebString& title,
      WebKit::WebTextDirection direction) OVERRIDE;
  virtual void didFailLoad(
      WebKit::WebFrame*, const WebKit::WebURLError&) OVERRIDE;
  virtual void didFinishLoad(WebKit::WebFrame*) OVERRIDE;
  virtual void didNavigateWithinPage(
      WebKit::WebFrame*, bool isNewNavigation) OVERRIDE;
  virtual void willSendRequest(
      WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&,
      const WebKit::WebURLResponse& redirectResponse) OVERRIDE;
  virtual void didChangeContentsSize(
      WebKit::WebFrame*, const WebKit::WebSize&) OVERRIDE;
  virtual void reportFindInPageMatchCount(
      int request_id, int count, bool final_update) OVERRIDE;
  virtual void reportFindInPageSelection(
      int request_id, int active_match_ordinal, const WebKit::WebRect& sel)
      OVERRIDE;
  virtual void openFileSystem(
      WebKit::WebFrame* frame,
      WebKit::WebFileSystem::Type type,
      long long size,  // NOLINT(runtime/int)
      bool create,
      WebKit::WebFileSystemCallbacks* callbacks) OVERRIDE;

  // WebKit::WebPermissionClient
  virtual bool allowScriptExtension(
      WebKit::WebFrame*,
      const WebKit::WebString& extensionName,
      int extensionGroup) OVERRIDE;

  // webkit_glue::WebPluginPageDelegate
  virtual webkit::npapi::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type) OVERRIDE;
  virtual void CreatedPluginWindow(
      gfx::PluginWindowHandle handle) OVERRIDE;
  virtual void WillDestroyPluginWindow(
      gfx::PluginWindowHandle handle) OVERRIDE;
  virtual void DidMovePlugin(
      const webkit::npapi::WebPluginGeometry& move) OVERRIDE;
  virtual void DidStartLoadingForPlugin() OVERRIDE {}
  virtual void DidStopLoadingForPlugin() OVERRIDE {}
  virtual WebKit::WebCookieJar* GetCookieJar() OVERRIDE;

  explicit BrowserWebViewDelegate(CefBrowserImpl* browser);
  virtual ~BrowserWebViewDelegate();
  void Reset();

  void SetSmartInsertDeleteEnabled(bool enabled);
  void SetSelectTrailingWhitespaceEnabled(bool enabled);

  // Additional accessors
#if defined(OS_WIN) || defined(OS_LINUX)
  // Sets the webview as a drop target.
  void RegisterDragDrop();
#endif
#if defined(OS_WIN)
  void RevokeDragDrop();

  // Called after dragging has finished.
  void EndDragging();

  BrowserDragDelegate* drag_delegate() { return drag_delegate_.get(); }
  WebDropTarget* drop_target() { return drop_target_.get(); }

  void set_destroy_on_drag_end(bool val) { destroy_on_drag_end_ = val; }
#endif  // defined(OS_WIN)

  void set_pending_extra_data(BrowserExtraData* extra_data) {
    pending_extra_data_.reset(extra_data);
  }

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

#ifdef OS_LINUX
  void HandleContextMenu(int selected_id);
#endif

  bool OnKeyboardEvent(const WebKit::WebKeyboardEvent& event,
                       bool isAfterJavaScript);

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
  void ShowStatus(const WebKit::WebString& text, cef_handler_statustype_t type);

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading.  From here, we can generate the dump for the
  // test.
  void LocationChangeDone(WebKit::WebFrame*);

  WebWidgetHost* GetWidgetHost();

  void UpdateForCommittedLoad(WebKit::WebFrame* webframe,
                              bool is_new_navigation);
  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateSessionHistory(WebKit::WebFrame* frame);

  bool OnBeforeMenu(const WebKit::WebContextMenuData& data,
                    int mouse_x,
                    int mouse_y,
                    int& edit_flags,
                    int& type_flags);

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
  bool destroy_on_drag_end_;
#endif
#if defined(OS_WIN) || defined(OS_LINUX)
  scoped_refptr<WebDropTarget> drop_target_;
#endif

#if defined(OS_LINUX)
  // The type of cursor the window is currently using.
  // Used for judging whether a new SetCursor call is actually changing the
  // cursor.
  GdkCursorType cursor_type_;
  scoped_refptr<WebDragSource> drag_source_;
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

  BrowserWebCookieJarImpl cookie_jar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWebViewDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_WEBVIEW_DELEGATE_H_
