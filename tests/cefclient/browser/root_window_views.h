// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_VIEWS_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_VIEWS_H_
#pragma once

#include <memory>
#include <string>

#include "tests/cefclient/browser/client_handler.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/views_window.h"

namespace client {

// Views framework implementation of a top-level window in the browser process.
// The methods of this class must be called on the main thread unless otherwise
// indicated.
class RootWindowViews : public RootWindow,
                        public ClientHandler::Delegate,
                        public ViewsWindow::Delegate {
 public:
  // Constructor may be called on any thread. |parent_window| will be
  // non-nullptr for popup browsers with a RootWindow parent (called on the UI
  // thread only).
  explicit RootWindowViews(bool use_alloy_style);
  ~RootWindowViews() override;

  void SetTitlebarHeight(const std::optional<float>& height);

  // RootWindow methods:
  bool IsViewsHosted() const override { return true; }
  void Init(RootWindow::Delegate* delegate,
            std::unique_ptr<RootWindowConfig> config,
            const CefBrowserSettings& settings) override;
  void InitAsPopup(RootWindow::Delegate* delegate,
                   bool with_controls,
                   bool with_osr,
                   const CefPopupFeatures& popupFeatures,
                   CefWindowInfo& windowInfo,
                   CefRefPtr<CefClient>& client,
                   CefBrowserSettings& settings) override;
  void Show(ShowMode mode) override;
  void Hide() override;
  void SetBounds(int x,
                 int y,
                 size_t width,
                 size_t height,
                 bool content_bounds) override;
  bool DefaultToContentBounds() const override;
  void Close(bool force) override;
  void SetDeviceScaleFactor(float device_scale_factor) override;
  std::optional<float> GetDeviceScaleFactor() const override;
  CefRefPtr<CefBrowser> GetBrowser() const override;
  ClientWindowHandle GetWindowHandle() const override;
  bool WithWindowlessRendering() const override { return false; }

  // ViewsWindow::Delegate methods:
  bool WithControls() override;
  bool InitiallyHidden() override;
  CefRefPtr<CefWindow> GetParentWindow() override;
  CefRect GetInitialBounds() override;
  cef_show_state_t GetInitialShowState() override;
  scoped_refptr<ImageCache> GetImageCache() override;
  void OnViewsWindowCreated(CefRefPtr<ViewsWindow> window) override;
  void OnViewsWindowClosing(CefRefPtr<ViewsWindow> window) override;
  void OnViewsWindowDestroyed(CefRefPtr<ViewsWindow> window) override;
  void OnViewsWindowActivated(CefRefPtr<ViewsWindow> window) override;
  ViewsWindow::Delegate* GetDelegateForPopup(
      CefRefPtr<CefClient> client) override;
  void OnTest(int test_id) override;
  void OnExit() override;

 protected:
  // ClientHandler::Delegate methods:
  bool UseViews() const override { return true; }
  bool UseAlloyStyle() const override { return IsAlloyStyle(); }
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBrowserClosing(CefRefPtr<CefBrowser> browser) override;
  void OnBrowserClosed(CefRefPtr<CefBrowser> browser) override;
  void OnSetAddress(const std::string& url) override;
  void OnSetTitle(const std::string& title) override;
  void OnSetFavicon(CefRefPtr<CefImage> image) override;
  void OnSetFullscreen(bool fullscreen) override;
  void OnAutoResize(const CefSize& new_size) override;
  void OnContentsBounds(const CefRect& new_bounds) override {
    RootWindow::SetBounds(new_bounds,
                          /*content_bounds=*/DefaultToContentBounds());
  }
  void OnSetLoadingState(bool isLoading,
                         bool canGoBack,
                         bool canGoForward) override;
  void OnSetDraggableRegions(
      const std::vector<CefDraggableRegion>& regions) override;
  bool OnSetFocus(cef_focus_source_t source) override;
  void OnTakeFocus(bool next) override;
  void OnBeforeContextMenu(CefRefPtr<CefMenuModel> model) override;

 private:
  void CreateClientHandler(const std::string& url);

  void InitOnUIThread(const CefBrowserSettings& settings,
                      CefRefPtr<CefRequestContext> request_context);
  void CreateViewsWindow(const CefBrowserSettings& settings,
                         CefRefPtr<CefRequestContext> request_context,
                         const ImageCache::ImageSet& images);

  void NotifyViewsWindowDestroyed();
  void NotifyViewsWindowActivated();
  void NotifyDestroyedIfDone();

  // Members set during initialization. Safe to access from any thread.
  std::unique_ptr<RootWindowConfig> config_;
  CefRefPtr<ClientHandler> client_handler_;

  // Only accessed on the main thread.
  CefRefPtr<CefBrowser> browser_;
  bool window_destroyed_ = false;
  bool browser_destroyed_ = false;

  // Only accessed on the browser process UI thread.
  CefRect initial_bounds_;
  cef_show_state_t initial_show_state_ = CEF_SHOW_STATE_NORMAL;
  bool position_on_resize_ = false;
  CefRefPtr<ViewsWindow> window_;
  scoped_refptr<ImageCache> image_cache_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowViews);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_VIEWS_H_
