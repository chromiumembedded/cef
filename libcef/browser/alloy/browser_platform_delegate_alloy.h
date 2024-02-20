// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_BROWSER_PLATFORM_DELEGATE_ALLOY_H_
#define CEF_LIBCEF_BROWSER_ALLOY_BROWSER_PLATFORM_DELEGATE_ALLOY_H_

#include "libcef/browser/alloy/dialogs/alloy_web_contents_dialog_helper.h"
#include "libcef/browser/browser_platform_delegate.h"

#include "base/memory/weak_ptr.h"
#include "components/find_in_page/find_notification_details.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

// Implementation of Alloy-based browser functionality.
class CefBrowserPlatformDelegateAlloy : public CefBrowserPlatformDelegate {
 public:
  CefBrowserPlatformDelegateAlloy(const CefBrowserPlatformDelegateAlloy&) =
      delete;
  CefBrowserPlatformDelegateAlloy& operator=(
      const CefBrowserPlatformDelegateAlloy&) = delete;

  content::WebContents* CreateWebContents(CefBrowserCreateParams& create_params,
                                          bool& own_web_contents) override;
  void WebContentsCreated(content::WebContents* web_contents,
                          bool owned) override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) override;
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_main_frame_navigation) override;
  void RenderViewReady() override;
  void BrowserCreated(CefBrowserHostBase* browser) override;
  void CreateExtensionHost(const extensions::Extension* extension,
                           const GURL& url,
                           extensions::mojom::ViewType host_type) override;
  extensions::ExtensionHost* GetExtensionHost() const override;
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      const override;
  void SendCaptureLostEvent() override;
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  void NotifyMoveOrResizeStarted() override;
#endif
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool IsNeverComposited(content::WebContents* web_contents) override;
  void SetAutoResizeEnabled(bool enabled,
                            const CefSize& min_size,
                            const CefSize& max_size) override;
  bool IsPrintPreviewSupported() const override;
  void Find(const CefString& searchText,
            bool forward,
            bool matchCase,
            bool findNext) override;
  void StopFinding(bool clearSelection) override;

  // Called from AlloyBrowserHostImpl::FindReply().
  bool HandleFindReply(int request_id,
                       int number_of_matches,
                       const gfx::Rect& selection_rect,
                       int active_match_ordinal,
                       bool final_update);

  const find_in_page::FindNotificationDetails& last_search_result() const {
    return last_search_result_;
  }

 protected:
  CefBrowserPlatformDelegateAlloy();

  base::RepeatingClosure GetBoundsChangedCallback();

  // Called from BrowserPlatformDelegateNative::set_windowless_handler().
  void set_as_secondary() { primary_ = false; }

 private:
  void SetOwnedWebContents(content::WebContents* owned_contents);

  void DestroyExtensionHost();
  void OnExtensionHostDeleted();

  void ConfigureAutoResize();

  // Non-nullptr if this object owns the WebContents. Will be nullptr for popup
  // browsers between the calls to WebContentsCreated() and AddNewContents(),
  // and may never be set if the parent browser is destroyed during popup
  // creation.
  std::unique_ptr<content::WebContents> owned_web_contents_;

  // Used for the print preview dialog.
  std::unique_ptr<AlloyWebContentsDialogHelper> web_contents_dialog_helper_;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc.
  find_in_page::FindNotificationDetails last_search_result_;

  // Used when the browser is hosting an extension.
  extensions::ExtensionHost* extension_host_ = nullptr;
  bool is_background_host_ = false;

  // Used with auto-resize.
  bool auto_resize_enabled_ = false;
  gfx::Size auto_resize_min_;
  gfx::Size auto_resize_max_;

  // True if this is the primary platform delegate, in which case it will
  // register WebContents delegate/observers.
  bool primary_ = true;

  base::WeakPtrFactory<CefBrowserPlatformDelegateAlloy> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_BROWSER_PLATFORM_DELEGATE_ALLOY_H_
