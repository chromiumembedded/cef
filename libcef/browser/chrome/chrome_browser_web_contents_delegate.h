// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_WEB_CONTENTS_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_WEB_CONTENTS_DELEGATE_H_
#pragma once

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"

// Adds CEF callbacks to Chrome's WebContentsDelegate implementation. Owned by
// BrowserWindowFeatures, with the same lifetime as BrowserWebContentsDelegate.
// Handles WebContents callbacks directly, with ChromeBrowserDelegate providing
// popup-host creation and shared browser/window state.
class ChromeBrowserWebContentsDelegate : public BrowserWebContentsDelegate {
 public:
  ChromeBrowserWebContentsDelegate(
      BrowserWindowInterface* browser,
      ExclusiveAccessManager& exclusive_access_manager,
      chrome::BrowserCommandController& command_controller,
      UnloadController& unload_controller,
      web_app::AppBrowserController* app_browser_controller,
      BrowserWindow& window,
      DesktopBrowserWindowCapabilities& capabilities,
      BrowserUiController& browser_ui_controller);

  ChromeBrowserWebContentsDelegate(const ChromeBrowserWebContentsDelegate&) =
      delete;
  ChromeBrowserWebContentsDelegate& operator=(
      const ChromeBrowserWebContentsDelegate&) = delete;

  ~ChromeBrowserWebContentsDelegate() override;

  // content::WebContentsDelegate methods:
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool PreHandleMouseEvent(content::WebContents* source,
                           const blink::WebMouseEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents,
      content::PreloadingTriggerType trigger_type) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool TakeFocus(content::WebContents* source, bool reverse) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          const content::GlobalRenderFrameHostId& opener_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;
  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
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
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

 private:
  const raw_ref<BrowserWindowInterface> browser_;
  // Match the base class's constructor-supplied app controller, which takes
  // precedence over CEF when handling draggable regions.
  const bool has_app_browser_controller_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_WEB_CONTENTS_DELEGATE_H_
