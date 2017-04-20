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
#include "include/views/cef_browser_view.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/file_dialog_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/javascript_dialog_manager.h"
#include "libcef/browser/menu_manager.h"
#include "libcef/common/response_manager.h"

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
struct DragEventSourceInfo;
class RenderWidgetHostImpl;
}

namespace net {
class URLRequest;
}

#if defined(USE_AURA)
namespace views {
class Widget;
}
#endif  // defined(USE_AURA)

struct Cef_DraggableRegion_Params;
struct Cef_Request_Params;
struct Cef_Response_Params;
class CefBrowserInfo;
class CefBrowserPlatformDelegate;
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
  class CommandResponseHandler : public virtual CefBaseRefCounted {
   public:
     virtual void OnResponse(const std::string& response) =0;
  };

  // Interface to implement for observers that wish to be informed of changes
  // to the CefBrowserHostImpl. All methods will be called on the UI thread.
  class Observer {
   public:
    // Called before |browser| is destroyed. Any references to |browser| should
    // be cleared when this method is called.
    virtual void OnBrowserDestroyed(CefBrowserHostImpl* browser) =0;

   protected:
    virtual ~Observer() {}
  };

  ~CefBrowserHostImpl() override;

  struct CreateParams {
    // Platform-specific window creation info. Will be nullptr when creating a
    // views-hosted browser.
    std::unique_ptr<CefWindowInfo> window_info;

#if defined(USE_AURA)
    // The BrowserView that will own a views-hosted browser. Will be nullptr for
    // popup browsers (the BrowserView will be created later in that case).
    CefRefPtr<CefBrowserView> browser_view;
#endif

    // Client implementation. May be nullptr.
    CefRefPtr<CefClient> client;

    // Initial URL to load. May be empty.
    CefString url;

    // Browser settings.
    CefBrowserSettings settings;

    // Other browser that opened this DevTools browser. Will be nullptr for non-
    // DevTools browsers.
    CefRefPtr<CefBrowserHostImpl> devtools_opener;

    // Request context to use when creating the browser. If nullptr the global
    // request context will be used.
    CefRefPtr<CefRequestContext> request_context;
  };

  // Create a new CefBrowserHostImpl instance.
  static CefRefPtr<CefBrowserHostImpl> Create(CreateParams& create_params);

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
  bool TryCloseBrowser() override;
  void SetFocus(bool focus) override;
  CefWindowHandle GetWindowHandle() override;
  CefWindowHandle GetOpenerWindowHandle() override;
  bool HasView() override;
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
  void DownloadImage(const CefString& image_url,
                     bool is_favicon,
                     uint32 max_image_size,
                     bool bypass_cache,
                     CefRefPtr<CefDownloadImageCallback> callback) override;
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
  bool HasDevTools() override;
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
  void ImeSetComposition(const CefString& text,
                         const std::vector<CefCompositionUnderline>& underlines,
                         const CefRange& replacement_range,
                         const CefRange& selection_range) override;
  void ImeCommitText(const CefString& text, const CefRange& replacement_range,
                     int relative_cursor_pos) override;
  void ImeFinishComposingText(bool keep_selection) override;
  void ImeCancelComposition() override;
  void DragTargetDragEnter(CefRefPtr<CefDragData> drag_data,
                           const CefMouseEvent& event,
                           DragOperationsMask allowed_ops) override;
  void DragTargetDragOver(const CefMouseEvent& event,
                          DragOperationsMask allowed_ops) override;
  void DragTargetDragLeave() override;
  void DragTargetDrop(const CefMouseEvent& event) override;
  void DragSourceSystemDragEnded() override;
  void DragSourceEndedAt(int x, int y, DragOperationsMask op) override;
  CefRefPtr<CefNavigationEntry> GetVisibleNavigationEntry() override;

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

  // Returns true if this browser is views-hosted.
  bool IsViewsHosted() const;

  // Called when the OS window hosting the browser is destroyed.
  void WindowDestroyed();

  // Destroy the browser members. This method should only be called after the
  // native browser window is not longer processing messages.
  void DestroyBrowser();

  // Cancel display of the context menu, if any.
  void CancelContextMenu();

#if defined(USE_AURA)
  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with views-
  // based browsers.
  CefRefPtr<CefBrowserView> GetBrowserView() const;
#endif

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
  void ViewText(const std::string& text);

  // Handler for URLs involving external protocols.
  void HandleExternalProtocol(const GURL& url);

  // Set the frame that currently has focus.
  void SetFocusedFrame(int64 frame_id);

  // Convert from view coordinates to screen coordinates. Potential display
  // scaling will be applied to the result.
  gfx::Point GetScreenPoint(const gfx::Point& view) const;

  void StartDragging(
      const content::DropData& drop_data,
      blink::WebDragOperationsMask allowed_ops,
      const gfx::ImageSkia& image,
      const gfx::Vector2d& image_offset,
      const content::DragEventSourceInfo& event_info,
      content::RenderWidgetHostImpl* source_rwh);
  void UpdateDragCursor(blink::WebDragOperation operation);

  // Thread safe accessors.
  const CefBrowserSettings& settings() const { return settings_; }
  SkColor GetBackgroundColor() const;
  CefRefPtr<CefClient> client() const { return client_; }
  scoped_refptr<CefBrowserInfo> browser_info() const { return browser_info_; }
  int browser_id() const;

  void OnSetFocus(cef_focus_source_t source);

  // Run the file chooser dialog specified by |params|. Only a single dialog may
  // be pending at any given time. |callback| will be executed asynchronously
  // after the dialog is dismissed or if another dialog is already pending.
  void RunFileChooser(
      const CefFileDialogRunner::FileChooserParams& params,
      const CefFileDialogRunner::RunFileChooserCallback& callback);

  bool HandleContextMenu(
      content::WebContents* web_contents,
      const content::ContextMenuParams& params);

  // Returns the WebContents most likely to handle an action. If extensions are
  // enabled and this browser has a full-page guest (for example, a full-page
  // PDF viewer extension) then the guest's WebContents will be returned.
  // Otherwise, the browser's WebContents will be returned.
  content::WebContents* GetActionableWebContents();

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
  bool DidAddMessageToConsole(content::WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool TakeFocus(content::WebContents* source,
                 bool reverse) override;
  bool HandleContextMenu(
      const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool CanDragEnter(
      content::WebContents* source,
      const content::DropData& data,
      blink::WebDragOperationsMask operations_allowed) override;
  void GetCustomWebContentsView(
      content::WebContents* web_contents,
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_frame_id,
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
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
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DocumentAvailableInMainFrame() override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description,
                   bool was_ignored_by_handler) override;
  void FrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnWebContentsFocused() override;
  // Override to provide a thread safe implementation.
  bool Send(IPC::Message* message) override;

  // Manage observer objects. The observer must either outlive this object or
  // remove itself before destruction. These methods can only be called on the
  // UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

 private:
  class DevToolsWebContentsObserver;

  static CefRefPtr<CefBrowserHostImpl> CreateInternal(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefBrowserHostImpl> opener,
      bool is_devtools_popup,
      CefRefPtr<CefRequestContext> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate);

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

  CefBrowserHostImpl(const CefBrowserSettings& settings,
                     CefRefPtr<CefClient> client,
                     content::WebContents* web_contents,
                     scoped_refptr<CefBrowserInfo> browser_info,
                     CefRefPtr<CefBrowserHostImpl> opener,
                     CefRefPtr<CefRequestContext> request_context,
                     std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate);

  // Give the platform delegate an opportunity to create the host window.
  bool CreateHostWindow();

  // Updates and returns an existing frame or creates a new frame. Pass
  // CefFrameHostImpl::kUnspecifiedFrameId for |parent_frame_id| if unknown.
  CefRefPtr<CefFrame> GetOrCreateFrame(int64 frame_id,
                                       int64 parent_frame_id,
                                       bool is_main_frame,
                                       base::string16 frame_name,
                                       const GURL& frame_url);
  // Remove the references to all frames and mark them as detached.
  void DetachAllFrames();

  void OnAddressChange(CefRefPtr<CefFrame> frame,
                       const GURL& url);
  void OnLoadStart(CefRefPtr<CefFrame> frame,
                   ui::PageTransition transition_type);
  void OnLoadError(CefRefPtr<CefFrame> frame,
                   const GURL& url,
                   int error_code);
  void OnLoadEnd(CefRefPtr<CefFrame> frame,
                 const GURL& url,
                 int http_status_code);
  void OnFullscreenModeChange(bool fullscreen);
  void OnTitleChange(const base::string16& title);

  void OnDevToolsWebContentsDestroyed();

  // Create the CefFileDialogManager if it doesn't already exist.
  void EnsureFileDialogManager();

  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  std::unique_ptr<content::WebContents> web_contents_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefWindowHandle opener_;
  CefRefPtr<CefRequestContext> request_context_;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate_;
  const bool is_windowless_;
  const bool is_views_hosted_;
  CefWindowHandle host_window_handle_;

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
  std::unique_ptr<content::NotificationRegistrar> registrar_;

  // Manages response registrations.
  std::unique_ptr<CefResponseManager> response_manager_;

  // Used for creating and managing file dialogs.
  std::unique_ptr<CefFileDialogManager> file_dialog_manager_;

  // Used for creating and managing JavaScript dialogs.
  std::unique_ptr<CefJavaScriptDialogManager> javascript_dialog_manager_;

  // Used for creating and managing context menus.
  std::unique_ptr<CefMenuManager> menu_manager_;

  // Track the lifespan of the frontend WebContents associated with this
  // browser.
  std::unique_ptr<DevToolsWebContentsObserver> devtools_observer_;
  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend_;

  // Observers that want to be notified of changes to this object.
  base::ObserverList<Observer> observers_;

  IMPLEMENT_REFCOUNTING(CefBrowserHostImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBrowserHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
