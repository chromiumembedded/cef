// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
#define CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "libcef/common/tracker.h"
#include "libcef/renderer/frame_impl.h"

#include "third_party/blink/public/web/web_view_observer.h"

namespace blink {
class WebFrame;
class WebNode;
}  // namespace blink

namespace content {
class RenderView;
}

// Renderer plumbing for CEF features. There is a one-to-one relationship
// between RenderView on the renderer side and RenderViewHost on the browser
// side.
//
// RenderViewObserver: Interface for observing RenderView notifications.
class CefBrowserImpl : public CefBrowser, public blink::WebViewObserver {
 public:
  // Returns the browser associated with the specified RenderView.
  static CefRefPtr<CefBrowserImpl> GetBrowserForView(content::RenderView* view);
  // Returns the browser associated with the specified main WebFrame.
  static CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(
      blink::WebFrame* frame);

  // CefBrowser methods.
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
  bool IsPopup() override;
  bool HasDocument() override;
  CefRefPtr<CefFrame> GetMainFrame() override;
  CefRefPtr<CefFrame> GetFocusedFrame() override;
  CefRefPtr<CefFrame> GetFrame(int64 identifier) override;
  CefRefPtr<CefFrame> GetFrame(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<int64>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;

  CefBrowserImpl(content::RenderView* render_view,
                 int browser_id,
                 bool is_popup,
                 bool is_windowless);

  CefBrowserImpl(const CefBrowserImpl&) = delete;
  CefBrowserImpl& operator=(const CefBrowserImpl&) = delete;

  ~CefBrowserImpl() override;

  // Returns the matching CefFrameImpl reference or creates a new one.
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(blink::WebLocalFrame* frame);
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(int64_t frame_id);

  // Frame objects will be deleted immediately before the frame is closed.
  void AddFrameObject(int64_t frame_id, CefTrackNode* tracked_object);

  int browser_id() const { return browser_id_; }
  bool is_popup() const { return is_popup_; }
  bool is_windowless() const { return is_windowless_; }

  // blink::WebViewObserver methods.
  void OnDestruct() override;
  void FrameDetached(int64_t frame_id);

  void OnLoadingStateChange(bool isLoading);

 private:
  // ID of the browser that this RenderView is associated with. During loading
  // of cross-origin requests multiple RenderViews may be associated with the
  // same browser ID.
  int browser_id_;
  bool is_popup_;
  bool is_windowless_;

  // Map of unique frame ids to CefFrameImpl references.
  using FrameMap = std::map<int64, CefRefPtr<CefFrameImpl>>;
  FrameMap frames_;

  // Map of unique frame ids to CefTrackManager objects that need to be cleaned
  // up when the frame is deleted.
  using FrameObjectMap = std::map<int64, CefRefPtr<CefTrackManager>>;
  FrameObjectMap frame_objects_;

  struct LoadingState {
    LoadingState(bool is_loading, bool can_go_back, bool can_go_forward)
        : is_loading_(is_loading),
          can_go_back_(can_go_back),
          can_go_forward_(can_go_forward) {}

    bool IsMatch(bool is_loading, bool can_go_back, bool can_go_forward) const {
      return is_loading_ == is_loading && can_go_back_ == can_go_back &&
             can_go_forward_ == can_go_forward;
    }

    bool is_loading_;
    bool can_go_back_;
    bool can_go_forward_;
  };
  std::unique_ptr<LoadingState> last_loading_state_;

  IMPLEMENT_REFCOUNTING(CefBrowserImpl);
};

#endif  // CEF_LIBCEF_RENDERER_BROWSER_IMPL_H_
