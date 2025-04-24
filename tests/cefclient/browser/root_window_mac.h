// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
#pragma once

#include <memory>
#include <string>

#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/root_window.h"

namespace client {

struct OsrRendererSettings;
class RootWindowMacImpl;

// OS X implementation of a top-level native window in the browser process.
// The methods of this class must be called on the main thread unless otherwise
// indicated.
class RootWindowMac : public RootWindow, public BrowserWindow::Delegate {
 public:
  // Constructor may be called on any thread.
  explicit RootWindowMac(bool use_alloy_style);
  ~RootWindowMac();

  BrowserWindow* browser_window() const;
  RootWindow::Delegate* delegate() const;
  const OsrRendererSettings* osr_settings() const;

  // RootWindow methods.
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
  bool WithWindowlessRendering() const override;

  // BrowserWindow::Delegate methods.
  bool UseAlloyStyle() const override { return IsAlloyStyle(); }
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBrowserWindowDestroyed() override;
  void OnSetAddress(const std::string& url) override;
  void OnSetTitle(const std::string& title) override;
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

  void OnNativeWindowClosed();

 private:
  CefRefPtr<RootWindowMacImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowMac);

  friend class RootWindowMacImpl;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
