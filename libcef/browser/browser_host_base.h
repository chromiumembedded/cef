// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "cef/include/cef_browser.h"
#include "cef/include/cef_client.h"
#include "cef/include/cef_unresponsive_process_callback.h"
#include "cef/include/views/cef_browser_view.h"
#include "cef/libcef/browser/browser_contents_delegate.h"
#include "cef/libcef/browser/browser_info.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/devtools/devtools_protocol_manager.h"
#include "cef/libcef/browser/devtools/devtools_window_runner.h"
#include "cef/libcef/browser/file_dialog_manager.h"
#include "cef/libcef/browser/frame_host_impl.h"
#include "cef/libcef/browser/javascript_dialog_manager.h"
#include "cef/libcef/browser/media_stream_registrar.h"
#include "cef/libcef/browser/request_context_impl.h"

class RenderViewContextMenuObserver;

// Parameters that are passed to the runtime-specific Create methods.
struct CefBrowserCreateParams {
  CefBrowserCreateParams() = default;

  // Copy constructor used with Chrome style only.
  CefBrowserCreateParams(const CefBrowserCreateParams& that) {
    operator=(that);
  }
  CefBrowserCreateParams& operator=(const CefBrowserCreateParams& that) {
    DCHECK(that.IsChromeStyle());

    // Not all parameters can be copied.
    client = that.client;
    url = that.url;
    settings = that.settings;
    request_context = that.request_context;
    extra_info = that.extra_info;
    if (that.window_info) {
      MaybeSetWindowInfo(*that.window_info, /*allow_alloy_style=*/false,
                         /*allow_chrome_style=*/true);
    }
    browser_view = that.browser_view;
    return *this;
  }

  // Initialize |window_info| with expected defaults before passing to a client
  // callback. |opener| will be non-nullptr for popups, DevTools windows, etc.
  static void InitWindowInfo(CefWindowInfo* window_info,
                             CefBrowserHostBase* opener);

  // Set |window_info| if appropriate (see below).
  void MaybeSetWindowInfo(const CefWindowInfo& window_info,
                          bool allow_alloy_style,
                          bool allow_chrome_style);

  // Returns true if |window_info| indicates creation of a Chrome style window.
  static bool IsChromeStyle(const CefWindowInfo* window_info);
  bool IsChromeStyle() const;

  // Returns true if parameters indicate windowless (off-screen) rendering.
  bool IsWindowless() const;

  // Platform-specific window creation info. Will be nullptr for Views-hosted
  // browsers except when using Chrome style with a native parent handle.
  std::unique_ptr<CefWindowInfo> window_info;

  // The BrowserView that will own a Views-hosted browser. Will be nullptr for
  // popup browsers.
  CefRefPtr<CefBrowserView> browser_view;

  // True if this browser is a popup and has a Views-hosted opener, in which
  // case the BrowserView for this browser will be created later (from
  // PopupWebContentsCreated).
  bool popup_with_views_hosted_opener = false;

  // True if this browser is a popup and has an Alloy style opener. Only used
  // with Chrome style.
  bool popup_with_alloy_style_opener = false;

  // Client implementation. May be nullptr.
  CefRefPtr<CefClient> client;

  // Initial URL to load. May be empty. If this is a valid extension URL then
  // the browser will be created as an app view extension host.
  CefString url;

  // Browser settings.
  CefBrowserSettings settings;

  // Request context to use when creating the browser. If nullptr the global
  // request context will be used.
  CefRefPtr<CefRequestContext> request_context;

  // Extra information that will be passed to
  // CefRenderProcessHandler::OnBrowserCreated.
  CefRefPtr<CefDictionaryValue> extra_info;
};

// Base class for CefBrowserHost implementations. Includes functionality that is
// shared by Alloy and Chrome styles. All methods are thread-safe unless
// otherwise indicated.
class CefBrowserHostBase : public CefBrowserHost,
                           public CefBrowser,
                           public CefBrowserContentsDelegate::Observer {
 public:
  // Interface to implement for observers that wish to be informed of changes
  // to the CefBrowserHostBase. All methods will be called on the UI thread.
  class Observer : public base::CheckedObserver {
   public:
    // Called before |browser| is destroyed. Any references to |browser| should
    // be cleared when this method is called.
    virtual void OnBrowserDestroyed(CefBrowserHostBase* browser) = 0;

   protected:
    ~Observer() override = default;
  };

  // Create a new CefBrowserHost instance of the current runtime type with
  // owned WebContents.
  static CefRefPtr<CefBrowserHostBase> Create(
      CefBrowserCreateParams& create_params);

  // Safe conversion from CefBrowserHostBase to CefBrowserHostBase.
  // Use this method instead of static_cast.
  static CefRefPtr<CefBrowserHostBase> FromBrowser(
      CefRefPtr<CefBrowser> browser);

  // Returns the browser associated with the specified RenderViewHost.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForHost(
      const content::RenderViewHost* host);
  // Returns the browser associated with the specified RenderFrameHost.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForHost(
      const content::RenderFrameHost* host);
  // Returns the browser associated with the specified WebContents.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForContents(
      const content::WebContents* contents);
  // Returns the browser associated with the specified global ID.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);
  // Returns the browser associated with the specified global token.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForGlobalToken(
      const content::GlobalRenderFrameHostToken& global_token);
  // Returns the browser associated with the specified top-level window.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForTopLevelNativeWindow(
      gfx::NativeWindow owning_window);
  // Returns the browser associated with the specified browser ID.
  static CefRefPtr<CefBrowserHostBase> GetBrowserForBrowserId(int browser_id);

  // Returns the browser most likely to be focused. This may be somewhat iffy
  // with windowless browsers as there is no guarantee that the client has only
  // one browser focused at a time.
  static CefRefPtr<CefBrowserHostBase> GetLikelyFocusedBrowser();

  CefBrowserHostBase(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefRequestContextImpl> request_context);

  CefBrowserHostBase(const CefBrowserHostBase&) = delete;
  CefBrowserHostBase& operator=(const CefBrowserHostBase&) = delete;

  // Called on the UI thread after the associated WebContents is created.
  virtual void InitializeBrowser();

  // Called on the UI thread when the OS window hosting the browser is
  // destroyed.
  virtual void WindowDestroyed() = 0;

  // Returns true if the browser is in the process of being destroyed. Called on
  // the UI thread only.
  virtual bool WillBeDestroyed() const = 0;

  // Called on the UI thread to complete WebContents tear-down. In most cases
  // this will be called via WebContentsObserver::WebContentsDestroyed. Any
  // remaining objects that reference the WebContents (including RFH, etc)
  // should be cleared in this callback.
  virtual void DestroyWebContents(content::WebContents* web_contents);

  // Called on the UI thread to complete CefBrowserHost tear-down.
  //
  // With Chrome style the WebContents is owned by the Browser's TabStripModel
  // and will usually be destroyed first: CloseBrowser -> (async) DoCloseBrowser
  // -> [TabStripModel deletes the WebContents] -> OnWebContentsDestroyed ->
  // DestroyWebContents -> (async) DestroyBrowser.
  //
  // With Alloy style the WebContents is owned by the
  // CefBrowserPlatformDelegateAlloy and will usually be destroyed at the same
  // time: CloseBrowser -> [OS/platform logic] -> (async) DestroyBrowser ->
  // [CefBrowserPlatformDelegateAlloy deletes the WebContents]
  // -> WebContentsDestroyed -> DestoyWebContents.
  //
  // There are a few exceptions to the above rules:
  // 1. If the CefBrowserHost still exists at CefShutdown, in which case
  //    DestroyBrowser will be called first via
  //    CefBrowserInfoManager::DestroyAllBrowsers.
  // 2. If a popup WebContents is still pending when the parent WebContents is
  //    destroyed, in which case WebContentsDestroyed will be called first via
  //    the parent WebContents destructor.
  virtual void DestroyBrowser();

  // CefBrowserHost methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefClient> GetClient() override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  bool CanZoom(cef_zoom_command_t command) override;
  void Zoom(cef_zoom_command_t command) override;
  double GetDefaultZoomLevel() override;
  double GetZoomLevel() override;
  void SetZoomLevel(double zoomLevel) override;
  bool HasView() override;
  bool IsReadyToBeClosed() override;
  void SetFocus(bool focus) override;
  int GetOpenerIdentifier() override;
  void RunFileDialog(FileDialogMode mode,
                     const CefString& title,
                     const CefString& default_file_path,
                     const std::vector<CefString>& accept_filters,
                     CefRefPtr<CefRunFileDialogCallback> callback) override;
  void StartDownload(const CefString& url) override;
  void DownloadImage(const CefString& image_url,
                     bool is_favicon,
                     uint32_t max_image_size,
                     bool bypass_cache,
                     CefRefPtr<CefDownloadImageCallback> callback) override;
  void Print() override;
  void PrintToPDF(const CefString& path,
                  const CefPdfPrintSettings& settings,
                  CefRefPtr<CefPdfPrintCallback> callback) override;
  void ShowDevTools(const CefWindowInfo& windowInfo,
                    CefRefPtr<CefClient> client,
                    const CefBrowserSettings& settings,
                    const CefPoint& inspect_element_at) override;
  void CloseDevTools() override;
  bool HasDevTools() override;
  void ReplaceMisspelling(const CefString& word) override;
  void AddWordToDictionary(const CefString& word) override;
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  bool SendDevToolsMessage(const void* message, size_t message_size) override;
  int ExecuteDevToolsMethod(int message_id,
                            const CefString& method,
                            CefRefPtr<CefDictionaryValue> params) override;
  CefRefPtr<CefRegistration> AddDevToolsMessageObserver(
      CefRefPtr<CefDevToolsMessageObserver> observer) override;
  void GetNavigationEntries(CefRefPtr<CefNavigationEntryVisitor> visitor,
                            bool current_only) override;
  CefRefPtr<CefNavigationEntry> GetVisibleNavigationEntry() override;
  void SetAudioMuted(bool mute) override;
  bool IsAudioMuted() override;
  void NotifyMoveOrResizeStarted() override;
  bool IsFullscreen() override;
  void ExitFullscreen(bool will_cause_resize) override;
  bool IsRenderProcessUnresponsive() override;
  cef_runtime_style_t GetRuntimeStyle() override;

  // CefBrowser methods:
  bool IsValid() override;
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
  bool HasDocument() override;
  bool IsPopup() override;
  CefRefPtr<CefFrame> GetMainFrame() override;
  CefRefPtr<CefFrame> GetFocusedFrame() override;
  CefRefPtr<CefFrame> GetFrameByIdentifier(
      const CefString& identifier) override;
  CefRefPtr<CefFrame> GetFrameByName(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<CefString>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;
  void SetAccessibilityState(cef_state_t accessibility_state) override;

  // CefBrowserContentsDelegate::Observer methods:
  void OnStateChanged(CefBrowserContentsState state_changed) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // Returns the frame object matching the specified |host| or nullptr if no
  // match is found. Must be called on the UI thread.
  CefRefPtr<CefFrame> GetFrameForHost(const content::RenderFrameHost* host);

  // Returns the frame associated with the specified global ID/token. See
  // documentation on RenderFrameHost::GetFrameTreeNodeId/Token() for why the
  // global ID/token is preferred.
  CefRefPtr<CefFrame> GetFrameForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);
  CefRefPtr<CefFrame> GetFrameForGlobalToken(
      const content::GlobalRenderFrameHostToken& global_token);

  // Manage observer objects. The observer must either outlive this object or
  // be removed before destruction. Must be called on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  // Methods called from CefFrameHostImpl.
  void LoadMainFrameURL(const content::OpenURLParams& params);
  virtual void OnSetFocus(cef_focus_source_t source) = 0;
  void ViewText(const std::string& text);

  // Calls CefFileDialogManager methods.
  void RunFileChooserForBrowser(
      const blink::mojom::FileChooserParams& params,
      CefFileDialogManager::RunFileChooserCallback callback);
  void RunSelectFile(ui::SelectFileDialog::Listener* listener,
                     std::unique_ptr<ui::SelectFilePolicy> policy,
                     ui::SelectFileDialog::Type type,
                     const std::u16string& title,
                     const base::FilePath& default_path,
                     const ui::SelectFileDialog::FileTypeInfo* file_types,
                     int file_type_index,
                     const base::FilePath::StringType& default_extension,
                     gfx::NativeWindow owning_window);
  void SelectFileListenerDestroyed(ui::SelectFileDialog::Listener* listener);

  // Called from AlloyBrowserHostImpl::GetJavaScriptDialogManager and
  // ChromeBrowserDelegate::GetJavaScriptDialogManager.
  content::JavaScriptDialogManager* GetJavaScriptDialogManager();

  // Called from CefBrowserInfoManager::MaybeAllowNavigation.
  virtual bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                                    const content::OpenURLParams& params);

  // Helpers for executing client callbacks. Must be called on the UI thread.
  void OnAfterCreated();
  void OnBeforeClose();
  void OnBrowserDestroyed();

  // Thread-safe accessors.
  const CefBrowserSettings& settings() const { return settings_; }
  CefRefPtr<CefClient> client() const { return client_; }
  scoped_refptr<CefBrowserInfo> browser_info() const { return browser_info_; }
  int browser_id() const;
  CefRefPtr<CefRequestContextImpl> request_context() const {
    return request_context_;
  }
  bool is_views_hosted() const { return is_views_hosted_; }
  SkColor GetBackgroundColor() const;

  // Returns true if windowless rendering is enabled.
  virtual bool IsWindowless() const = 0;

  // Returns the runtime style of this browser.
  virtual bool IsAlloyStyle() const = 0;
  bool IsChromeStyle() const { return !IsAlloyStyle(); }

  // Accessors that must be called on the UI thread.
  content::WebContents* GetWebContents() const;
  content::BrowserContext* GetBrowserContext() const;
  CefBrowserPlatformDelegate* platform_delegate() const {
    return platform_delegate_.get();
  }
  CefBrowserContentsDelegate* contents_delegate() {
    return &contents_delegate_;
  }
  CefMediaStreamRegistrar* GetMediaStreamRegistrar();
  CefDevToolsWindowRunner* GetDevToolsWindowRunner();

  CefRefPtr<CefUnresponsiveProcessCallback> unresponsive_process_callback()
      const {
    return unresponsive_process_callback_;
  }
  void set_unresponsive_process_callback(
      CefRefPtr<CefUnresponsiveProcessCallback> callback) {
    unresponsive_process_callback_ = callback;
  }

  RenderViewContextMenuObserver* context_menu_observer() const {
    return context_menu_observer_;
  }
  void set_context_menu_observer(RenderViewContextMenuObserver* observer) {
    context_menu_observer_ = observer;
  }

  // Returns the Widget owner for the browser window. Only used with windowed
  // browsers.
  views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with Views-
  // based browsers.
  CefRefPtr<CefBrowserView> GetBrowserView() const;

  // Returns the top-level native window for this browser. With windowed
  // browsers this will be an aura::Window* on Aura platforms (Windows/Linux)
  // and an NSWindow wrapper object from native_widget_types.h on MacOS. With
  // windowless browsers this method will always return an empty value.
  gfx::NativeWindow GetTopLevelNativeWindow() const;

  // Returns true if this browser is currently focused. A browser is considered
  // focused when the top-level RenderFrameHost is in the parent chain of the
  // currently focused RFH within the frame tree. In addition, its associated
  // RenderWidgetHost must also be focused. With windowed browsers only one
  // browser should be focused at a time. With windowless browsers this relies
  // on the client to properly configure focus state.
  bool IsFocused() const;

  // Returns true if this browser is currently visible.
  virtual bool IsVisible() const;

  // Returns the next popup ID for use with OnBeforePopup. Must be called on
  // the UI thread.
  int GetNextPopupId();

 protected:
  bool EnsureDevToolsProtocolManager();
  void InitializeDevToolsRegistrationOnUIThread(
      CefRefPtr<CefRegistration> registration);

  // Called from LoadMainFrameURL to perform the actual navigation.
  virtual bool Navigate(const content::OpenURLParams& params);

  // Called from ShowDevTools to perform the actual show.
  void ShowDevToolsOnUIThread(std::unique_ptr<CefShowDevToolsParams> params);

  // Create the CefFileDialogManager if it doesn't already exist.
  bool EnsureFileDialogManager();

  // Thread-safe members.
  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefRefPtr<CefRequestContextImpl> request_context_;
  const bool is_views_hosted_;
  int opener_id_ = 0;

  // Only accessed on the UI thread.
  CefBrowserContentsDelegate contents_delegate_;
  CefRefPtr<CefUnresponsiveProcessCallback> unresponsive_process_callback_;
  raw_ptr<RenderViewContextMenuObserver> context_menu_observer_ = nullptr;

  // Observers that want to be notified of changes to this object.
  // Only accessed on the UI thread.
  base::ObserverList<Observer> observers_;

  // Used for creating and managing file dialogs.
  std::unique_ptr<CefFileDialogManager> file_dialog_manager_;

  // Used for creating and managing JavaScript dialogs.
  std::unique_ptr<CefJavaScriptDialogManager> javascript_dialog_manager_;

  // Volatile state accessed from multiple threads. All access must be protected
  // by |state_lock_|.
  base::Lock state_lock_;
  bool is_loading_ = false;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
  bool has_document_ = false;
  bool is_fullscreen_ = false;
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // Used for managing DevTools instances without a frontend.
  std::unique_ptr<CefDevToolsProtocolManager> devtools_protocol_manager_;

  // Used for creating and running the DevTools window frontend.
  std::unique_ptr<CefDevToolsWindowRunner> devtools_window_runner_;

  std::unique_ptr<CefMediaStreamRegistrar> media_stream_registrar_;

  int next_popup_id_ = 1;

 private:
  IMPLEMENT_REFCOUNTING(CefBrowserHostBase);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
