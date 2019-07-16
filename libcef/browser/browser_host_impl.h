// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/views/cef_browser_view.h"
#include "libcef/browser/audio_mirror_destination.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/file_dialog_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/javascript_dialog_manager.h"
#include "libcef/browser/menu_manager.h"
#include "libcef/browser/request_context_impl.h"

#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/view_type.h"

namespace content {
struct DragEventSourceInfo;
class RenderWidgetHostImpl;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionHost;
}  // namespace extensions

#if defined(USE_AURA)
namespace views {
class Widget;
}
#endif  // defined(USE_AURA)

class CefAudioMirrorDestination;
class CefBrowserInfo;
class CefBrowserPlatformDelegate;
class CefDevToolsFrontend;
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
    virtual void OnResponse(const std::string& response) = 0;
  };

  // Interface to implement for observers that wish to be informed of changes
  // to the CefBrowserHostImpl. All methods will be called on the UI thread.
  class Observer {
   public:
    // Called before |browser| is destroyed. Any references to |browser| should
    // be cleared when this method is called.
    virtual void OnBrowserDestroyed(CefBrowserHostImpl* browser) = 0;

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

    // Initial URL to load. May be empty. If this is a valid extension URL then
    // the browser will be created as an app view extension host.
    GURL url;

    // Browser settings.
    CefBrowserSettings settings;

    // Other browser that opened this DevTools browser. Will be nullptr for non-
    // DevTools browsers.
    CefRefPtr<CefBrowserHostImpl> devtools_opener;

    // Request context to use when creating the browser. If nullptr the global
    // request context will be used.
    CefRefPtr<CefRequestContext> request_context;

    CefRefPtr<CefDictionaryValue> extra_info;

    // Used when explicitly creating the browser as an extension host via
    // ProcessManager::CreateBackgroundHost.
    const extensions::Extension* extension = nullptr;
    extensions::ViewType extension_host_type = extensions::VIEW_TYPE_INVALID;
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
  // Returns the browser associated with the specified FrameTreeNode ID.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForFrameTreeNode(
      int frame_tree_node_id);
  // Returns the browser associated with the specified frame routing IDs.
  static CefRefPtr<CefBrowserHostImpl> GetBrowserForFrameRoute(
      int render_process_id,
      int render_routing_id);

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
  void RunFileDialog(FileDialogMode mode,
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
  void Find(int identifier,
            const CefString& searchText,
            bool forward,
            bool matchCase,
            bool findNext) override;
  void StopFinding(bool clearSelection) override;
  void ShowDevTools(const CefWindowInfo& windowInfo,
                    CefRefPtr<CefClient> client,
                    const CefBrowserSettings& settings,
                    const CefPoint& inspect_element_at) override;
  void CloseDevTools() override;
  bool HasDevTools() override;
  void GetNavigationEntries(CefRefPtr<CefNavigationEntryVisitor> visitor,
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
  void SendExternalBeginFrame() override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  void SendFocusEvent(bool setFocus) override;
  void SendCaptureLostEvent() override;
  void NotifyMoveOrResizeStarted() override;
  int GetWindowlessFrameRate() override;
  void SetWindowlessFrameRate(int frame_rate) override;
  void ImeSetComposition(const CefString& text,
                         const std::vector<CefCompositionUnderline>& underlines,
                         const CefRange& replacement_range,
                         const CefRange& selection_range) override;
  void ImeCommitText(const CefString& text,
                     const CefRange& replacement_range,
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
  void SetAudioMuted(bool mute) override;
  bool IsAudioMuted() override;
  CefRefPtr<CefNavigationEntry> GetVisibleNavigationEntry() override;
  void SetAccessibilityState(cef_state_t accessibility_state) override;
  void SetAutoResizeEnabled(bool enabled,
                            const CefSize& min_size,
                            const CefSize& max_size) override;
  CefRefPtr<CefExtension> GetExtension() override;
  bool IsBackgroundHost() override;

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

  // Returns the frame associated with the specified RenderFrameHost.
  CefRefPtr<CefFrame> GetFrameForHost(const content::RenderFrameHost* host);

  // Returns the frame associated with the specified FrameTreeNode ID.
  CefRefPtr<CefFrame> GetFrameForFrameTreeNode(int frame_tree_node_id);

  // Load the specified URL in the main frame.
  void LoadMainFrameURL(const std::string& url,
                        const content::Referrer& referrer,
                        ui::PageTransition transition,
                        const std::string& extra_headers);

  // Called from CefFrameHostImpl.
  void OnFrameFocused(CefRefPtr<CefFrameHostImpl> frame);
  void OnDidFinishLoad(CefRefPtr<CefFrameHostImpl> frame,
                       const GURL& validated_url,
                       int http_status_code);

  // Open the specified text in the default text editor.
  void ViewText(const std::string& text);

  // Convert from view coordinates to screen coordinates. Potential display
  // scaling will be applied to the result.
  gfx::Point GetScreenPoint(const gfx::Point& view) const;

  void StartDragging(const content::DropData& drop_data,
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
  CefRefPtr<CefRequestContextImpl> request_context() const {
    return request_context_;
  }

  // Accessors that must be called on the UI thread.
  content::BrowserContext* GetBrowserContext();
  extensions::ExtensionHost* extension_host() const { return extension_host_; }

  void OnSetFocus(cef_focus_source_t source);

  // Run the file chooser dialog specified by |params|. Only a single dialog may
  // be pending at any given time. |callback| will be executed asynchronously
  // after the dialog is dismissed or if another dialog is already pending.
  void RunFileChooser(
      const CefFileDialogRunner::FileChooserParams& params,
      const CefFileDialogRunner::RunFileChooserCallback& callback);

  bool HandleContextMenu(content::WebContents* web_contents,
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
  bool ShouldTransferNavigation(bool is_main_frame_navigation) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void LoadProgressChanged(content::WebContents* source,
                           double progress) override;
  void CloseContents(content::WebContents* source) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void BeforeUnloadFired(content::WebContents* source,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;

  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
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
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  bool EmbedsFullscreenWidget() const override;
  void EnterFullscreenModeForTab(
      content::WebContents* web_contents,
      const GURL& origin,
      const blink::WebFullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override;
  blink::WebDisplayMode GetDisplayMode(
      const content::WebContents* web_contents) const override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void UpdatePreferredSize(content::WebContents* source,
                           const gfx::Size& pref_size) override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::MediaStreamType type) override;
  bool IsNeverVisible(content::WebContents* web_contents) override;

  // content::WebContentsObserver methods.
  using content::WebContentsObserver::BeforeUnloadFired;
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DocumentAvailableInMainFrame() override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& content_event_bundle) override;
  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& locData)
      override;

  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;

  // Manage observer objects. The observer must either outlive this object or
  // remove itself before destruction. These methods can only be called on the
  // UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  bool StartAudioMirroring();
  bool StopAudioMirroring();

  class NavigationLock final {
   private:
    friend class CefBrowserHostImpl;
    friend std::unique_ptr<NavigationLock>::deleter_type;

    explicit NavigationLock(CefRefPtr<CefBrowserHostImpl> browser);
    ~NavigationLock();

    CefRefPtr<CefBrowserHostImpl> browser_;
  };

  // Block navigation-related events on NavigationLock life span.
  std::unique_ptr<NavigationLock> CreateNavigationLock();

 private:
  class DevToolsWebContentsObserver;

  static CefRefPtr<CefBrowserHostImpl> CreateInternal(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      bool own_web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefBrowserHostImpl> opener,
      bool is_devtools_popup,
      CefRefPtr<CefRequestContextImpl> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      CefRefPtr<CefExtension> extension);

  // content::NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  CefBrowserHostImpl(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      content::WebContents* web_contents,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefBrowserHostImpl> opener,
      CefRefPtr<CefRequestContextImpl> request_context,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      CefRefPtr<CefExtension> extension);

  void set_owned_web_contents(content::WebContents* owned_contents);

  // Give the platform delegate an opportunity to create the host window.
  bool CreateHostWindow();

  // Create/delete the host for extensions.
  void CreateExtensionHost(const extensions::Extension* extension,
                           content::BrowserContext* browser_context,
                           content::WebContents* host_contents,
                           const GURL& url,
                           extensions::ViewType host_type);
  void DestroyExtensionHost();
  void OnExtensionHostDeleted();

  // Returns true if navigation actions are currently locked.
  bool navigation_locked() const;
  // Action to be executed once the navigation lock is released.
  void set_pending_navigation_action(base::OnceClosure action);

  void OnAddressChange(const GURL& url);
  void OnLoadStart(CefRefPtr<CefFrame> frame,
                   ui::PageTransition transition_type);
  void OnLoadError(CefRefPtr<CefFrame> frame, const GURL& url, int error_code);
  void OnLoadEnd(CefRefPtr<CefFrame> frame,
                 const GURL& url,
                 int http_status_code);
  void OnFullscreenModeChange(bool fullscreen);
  void OnTitleChange(const base::string16& title);

  void OnDevToolsWebContentsDestroyed();

  // Create the CefFileDialogManager if it doesn't already exist.
  void EnsureFileDialogManager();

  void ConfigureAutoResize();

  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefWindowHandle opener_;
  CefRefPtr<CefRequestContextImpl> request_context_;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate_;
  const bool is_windowless_;
  const bool is_views_hosted_;
  CefWindowHandle host_window_handle_;

  // Non-nullptr if this object owns the WebContents. Will be nullptr for popup
  // browsers between the calls to WebContentsCreated() and AddNewContents(),
  // and may never be set if the parent browser is destroyed during popup
  // creation.
  std::unique_ptr<content::WebContents> owned_web_contents_;

  // Volatile state information. All access must be protected by the state lock.
  base::Lock state_lock_;
  bool is_loading_;
  bool can_go_back_;
  bool can_go_forward_;
  bool has_document_;
  bool is_fullscreen_;

  // The currently focused frame, or nullptr if the main frame is focused.
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // Represents the current browser destruction state. Only accessed on the UI
  // thread.
  DestructionState destruction_state_;

  // Navigation will not occur while |navigation_lock_count_| > 0.
  // |pending_navigation_action_| will be executed when the lock is released.
  // Only accessed on the UI thread.
  int navigation_lock_count_ = 0;
  base::OnceClosure pending_navigation_action_;

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
  base::ObserverList<Observer>::Unchecked observers_;

  // Used to provide unique incremental IDs for each find request.
  int find_request_id_counter_ = 0;

  // Used when the browser is hosting an extension.
  extensions::ExtensionHost* extension_host_ = nullptr;
  CefRefPtr<CefExtension> extension_;
  bool is_background_host_ = false;

  // Used to mirror audio streams
  scoped_refptr<CefAudioMirrorDestination> audio_mirror_destination_;

  // Used with auto-resize.
  bool auto_resize_enabled_ = false;
  gfx::Size auto_resize_min_;
  gfx::Size auto_resize_max_;

  IMPLEMENT_REFCOUNTING(CefBrowserHostImpl);
  DISALLOW_COPY_AND_ASSIGN(CefBrowserHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_IMPL_H_
