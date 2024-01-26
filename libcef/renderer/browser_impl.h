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
#include "libcef/renderer/frame_impl.h"

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace blink {
class WebFrame;
class WebNode;
class WebView;
}  // namespace blink

// Renderer plumbing for CEF features. There is a one-to-one relationship
// between RenderView on the renderer side and RenderViewHost on the browser
// side.
//
// RenderViewObserver: Interface for observing RenderView notifications.
class CefBrowserImpl : public CefBrowser, public blink::WebViewObserver {
 public:
  // Returns the browser associated with the specified RenderView.
  static CefRefPtr<CefBrowserImpl> GetBrowserForView(blink::WebView* view);
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
  CefRefPtr<CefFrame> GetFrameByIdentifier(
      const CefString& identifier) override;
  CefRefPtr<CefFrame> GetFrameByName(const CefString& name) override;
  size_t GetFrameCount() override;
  void GetFrameIdentifiers(std::vector<CefString>& identifiers) override;
  void GetFrameNames(std::vector<CefString>& names) override;

  CefBrowserImpl(blink::WebView* web_view,
                 int browser_id,
                 bool is_popup,
                 bool is_windowless);

  CefBrowserImpl(const CefBrowserImpl&) = delete;
  CefBrowserImpl& operator=(const CefBrowserImpl&) = delete;

  ~CefBrowserImpl() override;

  // Returns the matching CefFrameImpl reference or creates a new one.
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(blink::WebLocalFrame* frame);
  CefRefPtr<CefFrameImpl> GetWebFrameImpl(const std::string& identifier);

  int browser_id() const { return browser_id_; }
  bool is_popup() const { return is_popup_; }
  bool is_windowless() const { return is_windowless_; }

  // blink::WebViewObserver methods.
  void OnDestruct() override;
  void FrameDetached(blink::WebLocalFrame* frame);

  void OnLoadingStateChange(bool isLoading);
  void OnEnterBFCache();

 private:
  // ID of the browser that this RenderView is associated with. During loading
  // of cross-origin requests multiple RenderViews may be associated with the
  // same browser ID.
  int browser_id_;
  bool is_popup_;
  bool is_windowless_;

  // Map of unique frame tokens to CefFrameImpl references.
  using FrameMap = std::map<blink::LocalFrameToken, CefRefPtr<CefFrameImpl>>;
  FrameMap frames_;

  // True if the browser was in the BFCache.
  bool was_in_bfcache_ = false;

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
