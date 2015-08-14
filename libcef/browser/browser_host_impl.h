// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
#pragma once

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/javascript_dialog_manager.h"
#include "libcef/browser/menu_creator.h"
#include "libcef/common/response_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/file_chooser_params.h"

#if defined(USE_AURA)
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

namespace content {
struct NativeWebKeyboardEvent;
}

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
class WebInputEvent;
}

namespace net {
class DirectoryLister;
class URLRequest;
}

#if defined(USE_AURA)
namespace views {
class Widget;
}
#endif

#if defined(USE_X11)
class CefWindowX11;
#endif

struct Cef_DraggableRegion_Params;
struct Cef_Request_Params;
struct Cef_Response_Params;
class CefBrowserInfo;
class CefDevToolsFrontend;
struct CefNavigateParams;
class SiteInstance;

// Implementation of CefBrowser.
//
// WebContentsDelegate: Interface for handling WebContents delegations. There is
// a one-to-one relationship between CefBrowserHostImpl and WebContents
// instances.
//
// WebContentsObserver: Interface for observing WebContents notifications and
// IPC messages. There is a one-to-one relationship between WebContents and
// RenderViewHost instances. IPC messages received by the RenderViewHost will be
// forwarded to this WebContentsObserver implementation via WebContents. IPC
// messages sent using CefBrowserHostImpl::Send() will be forwarded to the
// RenderViewHost (after posting to the UI thread if necessary). Use
// WebContentsObserver::routing_id() when sending IPC messages.
//
// NotificationObserver: Interface for observing post-processed notifications.
class CefBrowserHostImpl : public CefBrowserHost,
                           public CefBrowser,
                           public content::WebContentsDelegate,
                           public content::WebContentsObserver,
                           public content::NotificationObserver {
 public:
  // Used for handling the response to command messages.
  class CommandResponseHandler : public virtual CefBase {
   public:
     virtual void OnResponse(const std::string& response) =0;
  };

  // Extend content::FileChooserParams with some options unique to CEF.
  struct FileChooserParams : public content::FileChooserParams {
    // 0-based index of the selected value in |accept_types|.
    int selected_accept_filter = 0;

    // True if the Save dialog should prompt before overwriting files.
    bool overwriteprompt = true;

    // True if read-only files should be hidden.
    bool hidereadonly = true;
  };

  ~CefBrowserHostImpl() override;

  // Create a new CefBrowserHostImpl instance.
  static CefRefPtr<CefBrowserHostImpl> Create(
      const CefWindowInfo& windowInfo,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefWindowHandle opener,
      bool is_popup,
      CefRefPtr<CefRequestContext> request_context);

  // Returns the browser associated with the specified RenderViewHost.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForHost(
      const content::RenderViewHost* host);
  // Returns the browser associated with the specified RenderFrameHost.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForHost(
      const content::RenderFrameHost* host);
  // Returns the browser associated with the specified WebContents.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForContents(
      const content::WebContents* contents);
  // Returns the browser associated with the specified URLRequest.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForRequest(
      const net::URLRequest* request);
  // Returns the browser associated with the specified view routing IDs.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForView(
      int render_process_id, int render_routing_id);
  // Returns the browser associated with the specified frame routing IDs.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForFrame(
      int render_process_id, int render_routing_id);

  // CefBrowserHost methods.
  CefRefPtr<CefBrowser> GetBrowser() override;
  void CloseBrowser(bool force_close) override;
  void SetFocus(bool focus) override;
  void SetWindowVisibility(bool visible) override;
  CefWindowHandle GetWindowHandle() override;
  CefWindowHandle GetOpenerWindowHandle() override;
  CefRefPtr<CefClient> GetClient() override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  double GetZoomLevel() override;
  void SetZoomLevel(double zoomLevel) override;
  void RunFileDialog(
      FileDialogMode mode,
      const CefString& title,
      const CefString& default_file_path,
      const std::vector<CefString>& accept_filters,
      int selected_accept_filter,
      CefRefPtr<CefRunFileDialogCallback> callback) override;
  void StartDownload(const CefString& url) override;
  void Print() override;
  void PrintToPDF(const CefString& path,
                  const CefPdfPrintSettings& settings,
                  CefRefPtr<CefPdfPrintCallback> callback) override;
  void Find(int identifier, const CefString& searchText,
            bool forward, bool matchCase, bool findNext) override;
  void StopFinding(bool clearSelection) override;
  void ShowDevTools(const CefWindowInfo& windowInfo,
                    CefRefPtr<CefClient> client,
                    const CefBrowserSettings& settings,
                    const CefPoint& inspect_element_at) override;
  void CloseDevTools() override;
  void GetNavigationEntries(
      CefRefPtr<CefNavigationEntryVisitor> visitor,
      bool current_only) override;
  void SetMouseCursorChangeDisabled(bool disabled) override;
  bool IsMouseCursorChangeDisabled() override;
  bool IsWindowRenderingDisabled() override;
  void ReplaceMisspelling(const CefString& word) override;
  void AddWordToDictionary(const CefString& word) override;
  void WasResized() override;
  void WasHidden(bool hidden) override;
  void NotifyScreenInfoChanged() override;
  void Invalidate(PaintElementType type) override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           MouseButtonType type,
                           bool mouseUp, int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event,
                          bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX, int deltaY) override;
  void SendFocusEvent(bool setFocus) override;
  void SendCaptureLostEvent() override;
  void NotifyMoveOrResizeStarted() override;
  int GetWindowlessFrameRate() override;
  void SetWindowlessFrameRate(int frame_rate) override;
  CefTextInputContext GetNSTextInputContext() override;
  void HandleKeyEventBeforeTextInputClient(CefEventHandle keyEvent) override;
  void HandleKeyEventAfterTextInputClient(CefEventHandle keyEvent) override;
  void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                           const CefMouseEvent& event,
                           DragOperationsMask allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          DragOperationsMask allowed_ops) override;
  void DragTargetDragLeave() override;
  void DragTargetDrop(const CefMouseEvent& event) override;
  void DragSourceSystemDragEnded() override;
  void DragSourceEndedAt(int x, int y, DragOperationsMask op) override;

  // CefBrowser methods.
  CefRefPtr<CefBrowserHost> GetHost() override;
  bool CanGoBack() override;
  void GoBack() override;
  bool CanGoForward() override;
  void GoForward() override;
  bool IsLoading() override;
  void Reload() override;
  void ReloadIgnoreCache() override;
  void StopLoad() override;
  int GetIdentifier() override;
  bool IsSame(CefRefPtr<CefBrowser> that) override;
  bool IsPopup() override;
  bool HasDocument() override;
  CefRefPtr<CefFrame> GetMainFrame() override;
  CefRefPtr<CefFrame> GetFocusedFrame() override;
  CefRefPtr<CefFrame> GetFrame(int64 identifier) override;
  CefRefPtr<CefFrame> GetFrame(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<int64>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;
  bool SendProcessMessage(
      CefProcessId target_process,
      CefRefPtr<CefProcessMessage> message) override;

  // Returns true if windowless rendering is enabled.
  bool IsWindowless() const;
  // Returns true if transparent painting is enabled.
  bool IsTransparent() const;

  // Called when the OS window hosting the browser is destroyed.
  void WindowDestroyed();

  // Destroy the browser members. This method should only be called after the
  // native browser window is not longer processing messages.
  void DestroyBrowser();

  // Cancel display of the context menu, if any.
  void CancelContextMenu();

  // Returns the native view for the WebContents.
  gfx::NativeView GetContentView() const;

  // Returns a pointer to the WebContents.
  content::WebContents* GetWebContents() const;

  // Returns the frame associated with the specified URLRequest.
  CefRefPtr<CefFrame> GetFrameForRequest(net::URLRequest* request);

  // Navigate as specified by the |params| argument.
  void Navigate(const CefNavigateParams& params);

  // Load the specified request.
  void LoadRequest(int64 frame_id, CefRefPtr<CefRequest> request);

  // Load the specified URL.
  void LoadURL(int64 frame_id,
               const std::string& url,
               const content::Referrer& referrer,
               ui::PageTransition transition,
               const std::string& extra_headers);

  // Load the specified string.
  void LoadString(int64 frame_id, const std::string& string,
                  const std::string& url);

  // Send a command to the renderer for execution.
  void SendCommand(int64 frame_id, const std::string& command,
                   CefRefPtr<CefResponseManager::Handler> responseHandler);

  // Send code to the renderer for execution.
  void SendCode(int64 frame_id, bool is_javascript, const std::string& code,
                const std::string& script_url, int script_start_line,
                CefRefPtr<CefResponseManager::Handler> responseHandler);

  bool SendProcessMessage(CefProcessId target_process,
                          const std::string& name,
                          base::ListValue* arguments,
                          bool user_initiated);

  // Open the specified text in the default text editor.
  bool ViewText(const std::string& text);

  // Handler for URLs involving external protocols.
  void HandleExternalProtocol(const GURL& url);

  // Set the frame that currently has focus.
  void SetFocusedFrame(int64 frame_id);

  // Thread safe accessors.
  const CefBrowserSettings& settings() const { return settings_; }
  CefRefPtr<CefClient> client() const { return client_; }
  int browser_id() const;

#if defined(USE_AURA)
  views::Widget* window_widget() const { return window_widget_; }
#endif

#if defined(USE_X11)
  CefWindowX11* window_x11() const { return window_x11_; }
#endif

#if defined(OS_WIN)
  static void RegisterWindowClass();
#endif

#if defined(USE_AURA)
  ui::PlatformCursor GetPlatformCursor(blink::WebCursorInfo::Type type);
#endif

  void OnSetFocus(cef_focus_source_t source);

  // The argument vector will be empty if the dialog was cancelled.
  typedef base::Callback<void(int, const std::vector<base::FilePath>&)>
      RunFileChooserCallback;

  // Run the file chooser dialog specified by |params|. Only a single dialog may
  // be pending at any given time. |callback| will be executed asynchronously
  // after the dialog is dismissed or if another dialog is already pending.
  void RunFileChooser(const FileChooserParams& params,
                      const RunFileChooserCallback& callback);

  bool HandleContextMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params);

  // Returns the WebContents most likely to handle an action. If extensions are
  // enabled and this browser has a full-page guest (for example, a full-page
  // PDF viewer extension) then the guest's WebContents will be returned.
  // Otherwise, the browser's WebContents will be returned.
  content::WebContents* GetActionableWebContents();

  // Used when creating a new popup window.
  struct PendingPopupInfo {
    CefWindowInfo window_info;
    CefBrowserSettings settings;
    CefRefPtr<CefClient> client;
  };
  // Returns false if a popup is already pending.
  bool SetPendingPopupInfo(scoped_ptr<PendingPopupInfo> info);

  enum DestructionState {
    DESTRUCTION_STATE_NONE = 0,
    DESTRUCTION_STATE_PENDING,
    DESTRUCTION_STATE_ACCEPTED,
    DESTRUCTION_STATE_COMPLETED
  };
  DestructionState destruction_state() const { return destruction_state_; }

  // content::WebContentsDelegate methods.
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void CloseContents(content::WebContents* source) override;
  void UpdateTargetURL(content::WebContents* source,
                       const GURL& url) override;
  bool AddMessageToConsole(content::WebContents* source,
                           int32 level,
                           const base::string16& message,
                           int32 line_no,
                           const base::string16& source_id) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool TakeFocus(content::WebContents* source,
                 bool reverse) override;
  bool HandleContextMenu(
      const content::ContextMenuParams& params) override;
  bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool CanDragEnter(
      content::WebContents* source,
      const content::DropData& data,
      blink::WebDragOperationsMask operations_allowed) override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      int main_frame_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace,
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void RunFileChooser(
      content::WebContents* web_contents,
      const content::FileChooserParams& params) override;
  bool EmbedsFullscreenWidget() const override;
  void EnterFullscreenModeForTab(content::WebContents* web_contents,
                                 const GURL& origin) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;
  blink::WebDisplayMode GetDisplayMode(
      const content::WebContents* web_contents) const override;
  void FindReply(
      content::WebContents* web_contents,
      int request_id,
      int number_of_matches,
      const gfx::Rect& selection_rect,
      int active_match_ordinal,
      bool final_update) override;
  void UpdatePreferredSize(content::WebContents* source,
                           const gfx::Size& pref_size) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;

  // content::WebContentsObserver methods.
  using content::WebContentsObserver::BeforeUnloadFired;
  using content::WebContentsObserver::WasHidden;
  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override;
  void RenderViewCreated(
      content::RenderViewHost* render_view_host) override;
  void RenderViewDeleted(
      content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) override;
  void DidFailProvisionalLoad(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler) override;
  void DocumentAvailableInMainFrame() override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override;
  void FrameDeleted(
      content::RenderFrameHost* render_frame_host) override;
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnWebContentsFocused() override;
  // Override to provide a thread safe implementation.
  bool Send(IPC::Message* message) override;

 private:
  class DevToolsWebContentsObserver;

  static CefRefPtr<CefBrowserHostImpl> CreateInternal(
      const CefWindowInfo& window_info,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefWindowHandle opener,
      CefRefPtr<CefRequestContext> request_context);

  // content::WebContentsObserver::OnMessageReceived() message handlers.
  void OnFrameIdentified(int64 frame_id,
                         int64 parent_frame_id,
                         base::string16 name);
  void OnDidFinishLoad(
      int64 frame_id,
      const GURL& validated_url,
      bool is_main_frame,
      int http_status_code);
  void OnUpdateDraggableRegions(
      const std::vector<Cef_DraggableRegion_Params>& regions);
  void OnRequest(const Cef_Request_Params& params);
  void OnResponse(const Cef_Response_Params& params);
  void OnResponseAck(int request_id);

  // content::NotificationObserver methods.
  void Observe(int type,
              const content::NotificationSource& source,
              const content::NotificationDetails& details) override;

  CefBrowserHostImpl(const CefWindowInfo& window_info,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefClient> client,
                     content::WebContents* web_contents,
                     scoped_refptr<CefBrowserInfo> browser_info,
                     CefWindowHandle opener,
                     CefRefPtr<CefRequestContext> request_context);

  // Updates and returns an existing frame or creates a new frame. Pass
  // CefFrameHostImpl::kUnspecifiedFrameId for |parent_frame_id| if unknown.
  CefRefPtr<CefFrame> GetOrCreateFrame(int64 frame_id,
                                       int64 parent_frame_id,
                                       bool is_main_frame,
                                       base::string16 frame_name,
                                       const GURL& frame_url);
  // Remove the references to all frames and mark them as detached.
  void DetachAllFrames();

#if defined(OS_WIN)
  static LPCTSTR GetWndClass();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                                  WPARAM wParam, LPARAM lParam);
#endif

  // Create the window.
  bool PlatformCreateWindow();
  // Sends a message via the OS to close the native browser window.
  // DestroyBrowser will be called after the native window has closed.
  void PlatformCloseWindow();
  // Resize the window to the given dimensions.
  void PlatformSizeTo(int width, int height);
  // Set or remove focus from the window.
  void PlatformSetFocus(bool focus);
#if defined(OS_MACOSX)
  // Set or remove window visibility.
  void PlatformSetWindowVisibility(bool visible);
#endif
  // Return the handle for this window.
  CefWindowHandle PlatformGetWindowHandle();
  // Open the specified text in the default text editor.
  bool PlatformViewText(const std::string& text);
  // Forward the keyboard event to the application or frame window to allow
  // processing of shortcut keys.
  void PlatformHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event);
  // Invoke platform specific handling for the external protocol.
  void PlatformHandleExternalProtocol(const GURL& url);
  // Invoke platform specific file chooser dialog.
  void PlatformRunFileChooser(const FileChooserParams& params,
                              RunFileChooserCallback callback);

  void PlatformTranslateKeyEvent(content::NativeWebKeyboardEvent& native_event,
                                 const CefKeyEvent& key_event);
  void PlatformTranslateClickEvent(blink::WebMouseEvent& web_event,
                                   const CefMouseEvent& mouse_event,
                                   CefBrowserHost::MouseButtonType type,
                                   bool mouseUp, int clickCount);
  void PlatformTranslateMoveEvent(blink::WebMouseEvent& web_event,
                                  const CefMouseEvent& mouse_event,
                                  bool mouseLeave);
  void PlatformTranslateWheelEvent(blink::WebMouseWheelEvent& web_event,
                                   const CefMouseEvent& mouse_event,
                                   int deltaX, int deltaY);
  void PlatformTranslateMouseEvent(blink::WebMouseEvent& web_event,
                                   const CefMouseEvent& mouse_event);

  void PlatformNotifyMoveOrResizeStarted();

  int TranslateModifiers(uint32 cefKeyStates);
  void SendMouseEvent(const blink::WebMouseEvent& web_event);

  void OnAddressChange(CefRefPtr<CefFrame> frame,
                       const GURL& url);
  void OnLoadStart(CefRefPtr<CefFrame> frame,
                   const GURL& url,
                   ui::PageTransition transition_type);
  void OnLoadError(CefRefPtr<CefFrame> frame,
                   const GURL& url,
                   int error_code,
                   const base::string16& error_description);
  void OnLoadEnd(CefRefPtr<CefFrame> frame,
                 const GURL& url,
                 int http_status_code);
  void OnFullscreenModeChange(bool fullscreen);
  void OnTitleChange(const base::string16& title);

  // Continuation from RunFileChooser.
  void RunFileChooserOnUIThread(const FileChooserParams& params,
                                const RunFileChooserCallback& callback);

  // Used with RunFileChooser to clear the |file_chooser_pending_| flag.
  void OnRunFileChooserCallback(const RunFileChooserCallback& callback,
                                int selected_accept_filter,
                                const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser when mode is
  // content::FileChooserParams::UploadFolder.
  void OnRunFileChooserUploadFolderDelegateCallback(
      content::WebContents* web_contents,
      const content::FileChooserParams::Mode mode,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  // Used with WebContentsDelegate::RunFileChooser to notify the WebContents.
  void OnRunFileChooserDelegateCallback(
      content::WebContents* web_contents,
      content::FileChooserParams::Mode mode,
      int selected_accept_filter,
      const std::vector<base::FilePath>& file_paths);

  void OnDevToolsWebContentsDestroyed();

  CefWindowInfo window_info_;
  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefWindowHandle opener_;
  CefRefPtr<CefRequestContext> request_context_;

  // Pending popup information. Access must be protected by
  // |pending_popup_info_lock_|.
  base::Lock pending_popup_info_lock_;
  scoped_ptr<PendingPopupInfo> pending_popup_info_;

  // Volatile state information. All access must be protected by the state lock.
  base::Lock state_lock_;
  bool is_loading_;
  bool can_go_back_;
  bool can_go_forward_;
  bool has_document_;
  bool is_fullscreen_;

  // Messages we queue while waiting for the RenderView to be ready. We queue
  // them here instead of in the RenderProcessHost to ensure that they're sent
  // after the CefRenderViewObserver has been created on the renderer side.
  std::queue<IPC::Message*> queued_messages_;
  bool queue_messages_;

  // Map of unique frame ids to CefFrameHostImpl references.
  typedef std::map<int64, CefRefPtr<CefFrameHostImpl> > FrameMap;
  FrameMap frames_;
  // The unique frame id currently identified as the main frame.
  int64 main_frame_id_;
  // The unique frame id currently identified as the focused frame.
  int64 focused_frame_id_;
  // Used when no other frame exists. Provides limited functionality.
  CefRefPtr<CefFrameHostImpl> placeholder_frame_;

  // Represents the current browser destruction state. Only accessed on the UI
  // thread.
  DestructionState destruction_state_;

  // True if frame destruction is currently pending. Navigation should not occur
  // while this flag is true.
  bool frame_destruction_pending_;

  // True if the OS window hosting the browser has been destroyed. Only accessed
  // on the UI thread.
  bool window_destroyed_;

  // True if currently in the OnSetFocus callback. Only accessed on the UI
  // thread.
  bool is_in_onsetfocus_;

  // True if the focus is currently on an editable field on the page. Only
  // accessed on the UI thread.
  bool focus_on_editable_field_;

  // True if mouse cursor change is disabled.
  bool mouse_cursor_change_disabled_;

  // Used for managing notification subscriptions.
  scoped_ptr<content::NotificationRegistrar> registrar_;

  // Manages response registrations.
  scoped_ptr<CefResponseManager> response_manager_;

  // Used for creating and managing JavaScript dialogs.
  scoped_ptr<CefJavaScriptDialogManager> dialog_manager_;

  // Used for creating and managing context menus.
  scoped_ptr<CefMenuCreator> menu_creator_;

  // Track the lifespan of the frontend WebContents associated with this
  // browser.
  scoped_ptr<DevToolsWebContentsObserver> devtools_observer_;
  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend_;

  // True if a file chooser is currently pending.
  bool file_chooser_pending_;

  // Used for asynchronously listing directory contents.
  scoped_ptr<net::DirectoryLister> lister_;

#if defined(USE_AURA)
  // Widget hosting the web contents. It will be deleted automatically when the
  // associated root window is destroyed.
  views::Widget* window_widget_;
#endif  // defined(USE_AURA)
#if defined(USE_X11)
  CefWindowX11* window_x11_;
  scoped_ptr<ui::XScopedCursor> invisible_cursor_;
#endif  // defined(USE_X11)

  IMPLEMENT_REFCOUNTING(CefBrowserHostImpl);
  DISALLOW_EVIL_CONSTRUCTORS(CefBrowserHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
