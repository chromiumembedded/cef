// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
#pragma once

#include <memory>

#include "libcef/browser/frame_host_impl.h"

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class CefBrowser;
class CefBrowserInfo;
class CefBrowserPlatformDelegate;
class CefClient;

// Flags that represent which states have changed.
enum class CefBrowserContentsState : uint8_t {
  kNone = 0,
  kNavigation = (1 << 0),
  kDocument = (1 << 1),
  kFullscreen = (1 << 2),
  kFocusedFrame = (1 << 3),
};

constexpr inline CefBrowserContentsState operator&(
    CefBrowserContentsState lhs,
    CefBrowserContentsState rhs) {
  return static_cast<CefBrowserContentsState>(static_cast<int>(lhs) &
                                              static_cast<int>(rhs));
}

constexpr inline CefBrowserContentsState operator|(
    CefBrowserContentsState lhs,
    CefBrowserContentsState rhs) {
  return static_cast<CefBrowserContentsState>(static_cast<int>(lhs) |
                                              static_cast<int>(rhs));
}

// Tracks state and executes client callbacks based on WebContents callbacks.
// Includes functionality that is shared by the alloy and chrome runtimes.
// Only accessed on the UI thread.
class CefBrowserContentsDelegate : public content::WebContentsDelegate,
                                   public content::WebContentsObserver,
                                   public content::NotificationObserver {
 public:
  using State = CefBrowserContentsState;

  // Interface to implement for observers that wish to be informed of changes
  // to the delegate. All methods will be called on the UI thread.
  class Observer : public base::CheckedObserver {
   public:
    // Called after state has changed and before the associated CefClient
    // callback is executed.
    virtual void OnStateChanged(State state_changed) = 0;

    // Called when the associated WebContents is destroyed.
    virtual void OnWebContentsDestroyed(content::WebContents* web_contents) = 0;

   protected:
    ~Observer() override {}
  };

  explicit CefBrowserContentsDelegate(
      scoped_refptr<CefBrowserInfo> browser_info);

  void ObserveWebContents(content::WebContents* new_contents);

  // Manage observer objects. The observer must either outlive this object or
  // be removed before destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // WebContentsDelegate methods:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void DidNavigatePrimaryMainFramePostCommit(
      content::WebContents* web_contents) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;


void HandleClipboardChanged(const char* data,
                                                 size_t size) override;

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // WebContentsObserver methods:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderViewReady() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void OnFrameFocused(content::RenderFrameHost* render_frame_host) override;
  void DocumentAvailableInMainFrame(
      content::RenderFrameHost* render_frame_host) override;
  void LoadProgressChanged(double progress) override;
  void DidStopLoading() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void OnWebContentsFocused(
      content::RenderWidgetHost* render_widget_host) override;
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;
  void WebContentsDestroyed() override;

  // NotificationObserver methods.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Accessors for state information. Changes will be signaled to
  // Observer::OnStateChanged.
  bool is_loading() const { return is_loading_; }
  bool can_go_back() const { return can_go_back_; }
  bool can_go_forward() const { return can_go_forward_; }
  bool has_document() const { return has_document_; }
  bool is_fullscreen() const { return is_fullscreen_; }
  CefRefPtr<CefFrameHostImpl> focused_frame() const { return focused_frame_; }

  // Helpers for executing client callbacks.
  // TODO(cef): Make this private if/when possible.
  void OnLoadEnd(CefRefPtr<CefFrame> frame,
                 const GURL& url,
                 int http_status_code);
  bool OnSetFocus(cef_focus_source_t source);

 private:
  CefRefPtr<CefClient> client() const;
  CefRefPtr<CefBrowser> browser() const;
  CefBrowserPlatformDelegate* platform_delegate() const;

  // Helpers for executing client callbacks.
  void OnAddressChange(const GURL& url);
  void OnLoadStart(CefRefPtr<CefFrame> frame,
                   ui::PageTransition transition_type);
  void OnLoadError(CefRefPtr<CefFrame> frame, const GURL& url, int error_code);
  void OnTitleChange(const std::u16string& title);
  void OnFullscreenModeChange(bool fullscreen);

  void OnStateChanged(State state_changed);

  scoped_refptr<CefBrowserInfo> browser_info_;

  bool is_loading_ = false;
  bool can_go_back_ = false;
  bool can_go_forward_ = false;
  bool has_document_ = false;
  bool is_fullscreen_ = false;

  // The currently focused frame, or nullptr if the main frame is focused.
  CefRefPtr<CefFrameHostImpl> focused_frame_;

  // True if currently in the OnSetFocus callback.
  bool is_in_onsetfocus_ = false;

  // Observers that want to be notified of changes to this object.
  base::ObserverList<Observer> observers_;

  // Used for managing notification subscriptions.
  std::unique_ptr<content::NotificationRegistrar> registrar_;

  // True if the focus is currently on an editable field on the page.
  bool focus_on_editable_field_ = false;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContentsDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTENTS_DELEGATE_H_
