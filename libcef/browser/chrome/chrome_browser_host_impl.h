// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_HOST_IMPL_H_
#pragma once

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/chrome/browser_delegate.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"

class ChromeBrowserDelegate;
class ChromeBrowserView;

// CefBrowser implementation for the chrome runtime. Method calls are delegated
// to the chrome Browser object or the WebContents as appropriate. See the
// ChromeBrowserDelegate documentation for additional details. All methods are
// thread-safe unless otherwise indicated.
class ChromeBrowserHostImpl : public CefBrowserHostBase {
 public:
  // CEF-specific parameters passed via Browser::CreateParams::cef_params and
  // possibly shared by multiple Browser instances.
  class DelegateCreateParams : public cef::BrowserDelegate::CreateParams {
   public:
    explicit DelegateCreateParams(const CefBrowserCreateParams& create_params)
        : create_params_(create_params) {}

    CefBrowserCreateParams create_params_;
  };

  // Create a new Browser with a single tab (WebContents) and associated
  // ChromeBrowserHostImpl instance.
  static CefRefPtr<ChromeBrowserHostImpl> Create(
      const CefBrowserCreateParams& params);

  // Returns the browser associated with the specified RenderViewHost.
  static CefRefPtr<ChromeBrowserHostImpl> GetBrowserForHost(
      const content::RenderViewHost* host);
  // Returns the browser associated with the specified RenderFrameHost.
  static CefRefPtr<ChromeBrowserHostImpl> GetBrowserForHost(
      const content::RenderFrameHost* host);
  // Returns the browser associated with the specified WebContents.
  static CefRefPtr<ChromeBrowserHostImpl> GetBrowserForContents(
      const content::WebContents* contents);
  // Returns the browser associated with the specified global ID.
  static CefRefPtr<ChromeBrowserHostImpl> GetBrowserForGlobalId(
      const content::GlobalRenderFrameHostId& global_id);
  // Returns the browser associated with the specified Browser.
  static CefRefPtr<ChromeBrowserHostImpl> GetBrowserForBrowser(
      const Browser* browser);

  ~ChromeBrowserHostImpl() override;

  // CefBrowserContentsDelegate::Observer methods:
  void OnWebContentsDestroyed(content::WebContents* web_contents) override;

  // CefBrowserHostBase methods called from CefFrameHostImpl:
  void OnSetFocus(cef_focus_source_t source) override;

  // CefBrowserHost methods:
  void CloseBrowser(bool force_close) override;
  bool TryCloseBrowser() override;
  CefWindowHandle GetWindowHandle() override;
  CefWindowHandle GetOpenerWindowHandle() override;
  void Find(const CefString& searchText,
            bool forward,
            bool matchCase,
            bool findNext) override;
  void StopFinding(bool clearSelection) override;
  void CloseDevTools() override;
  bool HasDevTools() override;
  bool IsWindowRenderingDisabled() override;
  void WasResized() override;
  void WasHidden(bool hidden) override;
  void NotifyScreenInfoChanged() override;
  void Invalidate(PaintElementType type) override;
  void SendExternalBeginFrame() override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  void SendCaptureLostEvent() override;
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
  void SetAutoResizeEnabled(bool enabled,
                            const CefSize& min_size,
                            const CefSize& max_size) override;
  CefRefPtr<CefExtension> GetExtension() override;
  bool IsBackgroundHost() override;
  bool CanExecuteChromeCommand(int command_id) override;
  void ExecuteChromeCommand(int command_id,
                            cef_window_open_disposition_t disposition) override;

  Browser* browser() const { return browser_; }

  // Return the CEF specialization of BrowserView.
  ChromeBrowserView* chrome_browser_view() const;

  base::WeakPtr<ChromeBrowserHostImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  bool Navigate(const content::OpenURLParams& params) override;
  void ShowDevToolsOnUIThread(
      std::unique_ptr<CefShowDevToolsParams> params) override;

 private:
  friend class ChromeBrowserDelegate;

  ChromeBrowserHostImpl(
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
      scoped_refptr<CefBrowserInfo> browser_info,
      CefRefPtr<CefRequestContextImpl> request_context);

  // Create a new Browser without initializing the WebContents.
  // |browser_create_params| may be empty for default Browser creation behavior.
  static Browser* CreateBrowser(
      const CefBrowserCreateParams& params,
      std::optional<Browser::CreateParams> browser_create_params);

  // Called from ChromeBrowserDelegate::CreateBrowser when this object is first
  // created. Must be called on the UI thread.
  void Attach(content::WebContents* web_contents,
              bool is_devtools_popup,
              CefRefPtr<ChromeBrowserHostImpl> opener);

  // Called from ChromeBrowserDelegate::AddNewContents to take ownership of a
  // popup WebContents. |browser_create_params| may be empty for default Browser
  // creation behavior.
  void AddNewContents(
      std::unique_ptr<content::WebContents> contents,
      std::optional<Browser::CreateParams> browser_create_params);

  // Called when this object changes Browser ownership (e.g. initially created,
  // dragging between windows, etc). The old Browser, if any, will be cleared
  // before the new Browser is added. Must be called on the UI thread.
  void SetBrowser(Browser* browser);

  void SetDevToolsBrowserHost(
      base::WeakPtr<ChromeBrowserHostImpl> devtools_browser_host);

  // CefBrowserHostBase methods:
  void WindowDestroyed() override;
  bool WillBeDestroyed() const override;
  void DestroyBrowser() override;

  void DoCloseBrowser(bool force_close);

  // Returns the current tab index for the associated WebContents, or
  // TabStripModel::kNoTab if not found.
  int GetCurrentTabIndex() const;

  Browser* browser_ = nullptr;
  CefWindowHandle host_window_handle_ = kNullWindowHandle;

  base::WeakPtr<ChromeBrowserHostImpl> devtools_browser_host_;

  base::WeakPtrFactory<ChromeBrowserHostImpl> weak_ptr_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_HOST_IMPL_H_
