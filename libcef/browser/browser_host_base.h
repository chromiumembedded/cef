// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/views/cef_browser_view.h"
#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/devtools/devtools_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/browser/request_context_impl.h"

#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {
class Extension;
}

// Parameters that are passed to the runtime-specific Create methods.
struct CefBrowserCreateParams {
  CefBrowserCreateParams() {}

  // Copy constructor used with the chrome runtime only.
  CefBrowserCreateParams(const CefBrowserCreateParams& that) {
    operator=(that);
  }
  CefBrowserCreateParams& operator=(const CefBrowserCreateParams& that) {
    // Not all parameters can be copied.
    client = that.client;
    url = that.url;
    settings = that.settings;
    request_context = that.request_context;
    extra_info = that.extra_info;
#if defined(TOOLKIT_VIEWS)
    browser_view = that.browser_view;
#endif
    return *this;
  }

  // Platform-specific window creation info. Will be nullptr when creating a
  // views-hosted browser. Currently used with the alloy runtime only.
  std::unique_ptr<CefWindowInfo> window_info;

#if defined(TOOLKIT_VIEWS)
  // The BrowserView that will own a Views-hosted browser. Will be nullptr for
  // popup browsers.
  CefRefPtr<CefBrowserView> browser_view;

  // True if this browser is a popup and has a Views-hosted opener, in which
  // case the BrowserView for this browser will be created later (from
  // PopupWebContentsCreated).
  bool popup_with_views_hosted_opener = false;
#endif

  // Client implementation. May be nullptr.
  CefRefPtr<CefClient> client;

  // Initial URL to load. May be empty. If this is a valid extension URL then
  // the browser will be created as an app view extension host.
  CefString url;

  // Browser settings.
  CefBrowserSettings settings;

  // Other browser that opened this DevTools browser. Will be nullptr for non-
  // DevTools browsers. Currently used with the alloy runtime only.
  CefRefPtr<CefBrowserHostBase> devtools_opener;

  // Request context to use when creating the browser. If nullptr the global
  // request context will be used.
  CefRefPtr<CefRequestContext> request_context;

  // Extra information that will be passed to
  // CefRenderProcessHandler::OnBrowserCreated.
  CefRefPtr<CefDictionaryValue> extra_info;

  // Used when explicitly creating the browser as an extension host via
  // ProcessManager::CreateBackgroundHost. Currently used with the alloy
  // runtime only.
  const extensions::Extension* extension = nullptr;
  extensions::mojom::ViewType extension_host_type =
      extensions::mojom::ViewType::kInvalid;
};

// Base class for CefBrowserHost implementations. Includes functionality that is
// shared by the alloy and chrome runtimes. All methods are thread-safe unless
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
    virtual ~Observer() {}
  };

  // Create a new CefBrowserHost instance of the current runtime type with
  // owned WebContents.
  static CefRefPtr<CefBrowserHostBase> Create(
      CefBrowserCreateParams& create_params);

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

  // Called on the UI thread after the associated WebContents is destroyed.
  // Also called from CefBrowserInfoManager::DestroyAllBrowsers if the browser
  // was not properly shut down.
  virtual void DestroyBrowser();

  // CefBrowserHost methods:
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefClient> GetClient() override;
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  bool HasView() override;
  void StartDownload(const CefString& url) override;
  void DownloadImage(const CefString& image_url,
                     bool is_favicon,
                     uint32 max_image_size,
                     bool bypass_cache,
                     CefRefPtr<CefDownloadImageCallback> callback) override;
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
  CefRefPtr<CefFrame> GetFrame(int64 identifier) override;
  CefRefPtr<CefFrame> GetFrame(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<int64>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;

  // CefBrowserContentsDelegate::Observer methods:
  void OnStateChanged(CefBrowserContentsState state_changed) override;
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // Returns the frame associated with the specified RenderFrameHost.
  CefRefPtr<CefFrame> GetFrameForHost(const content::RenderFrameHost* host);

  // Returns the frame associated with the specified global ID. See
  // documentation on RenderFrameHost::GetFrameTreeNodeId() for why the global
  // ID is preferred.
  CefRefPtr<CefFrame> GetFrameForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);

  // Manage observer objects. The observer must either outlive this object or
  // be removed before destruction. Must be called on the UI thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  // Methods called from CefFrameHostImpl.
  void LoadMainFrameURL(const content::OpenURLParams& params);
  void OnDidFinishLoad(CefRefPtr<CefFrameHostImpl> frame,
                       const GURL& validated_url,
                       int http_status_code);
  virtual void OnSetFocus(cef_focus_source_t source) = 0;
  void ViewText(const std::string& text);

  // Called from CefBrowserInfoManager::MaybeAllowNavigation.
  virtual bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                                    bool is_guest_view,
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
  virtual bool IsWindowless() const;

  // Accessors that must be called on the UI thread.
  content::WebContents* GetWebContents() const;
  content::BrowserContext* GetBrowserContext() const;
  CefBrowserPlatformDelegate* platform_delegate() const {
    return platform_delegate_.get();
  }
  CefBrowserContentsDelegate* contents_delegate() const {
    return contents_delegate_.get();
  }

#if defined(TOOLKIT_VIEWS)
  // Returns the Widget owner for the browser window. Only used with windowed
  // rendering.
  views::Widget* GetWindowWidget() const;

  // Returns the BrowserView associated with this browser. Only used with Views-
  // based browsers.
  CefRefPtr<CefBrowserView> GetBrowserView() const;
#endif

 protected:
  bool EnsureDevToolsManager();
  void InitializeDevToolsRegistrationOnUIThread(
      CefRefPtr<CefRegistration> registration);

  // Called from LoadMainFrameURL to perform the actual navigation.
  virtual bool Navigate(const content::OpenURLParams& params);

  // Thread-safe members.
  CefBrowserSettings settings_;
  CefRefPtr<CefClient> client_;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate_;
  scoped_refptr<CefBrowserInfo> browser_info_;
  CefRefPtr<CefRequestContextImpl> request_context_;
  const bool is_views_hosted_;

  // Only accessed on the UI thread.
  std::unique_ptr<CefBrowserContentsDelegate> contents_delegate_;

  // Observers that want to be notified of changes to this object.
  // Only accessed on the UI thread.
  base::ObserverList<Observer> observers_;

  // Volatile state accessed from multiple threads. All access must be protected
  // by |state_lock_|.
  base::Lock state_lock_;
  bool is_loading_ = false;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
  bool has_document_ = false;
  bool is_fullscreen_ = false;
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // Used for creating and managing DevTools instances.
  std::unique_ptr<CefDevToolsManager> devtools_manager_;

 private:
  IMPLEMENT_REFCOUNTING(CefBrowserHostBase);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_HOST_BASE_H_
