// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
#pragma once

#include <string>

#include "include/base/cef_scoped_ptr.h"
#include "tests/cefclient/browser/browser_window.h"
#include "tests/cefclient/browser/root_window.h"

namespace client {

class RootWindowMacImpl;

// OS X implementation of a top-level native window in the browser process.
// The methods of this class must be called on the main thread unless otherwise
// indicated.
class RootWindowMac : public RootWindow, public BrowserWindow::Delegate {
 public:
  // Constructor may be called on any thread.
  RootWindowMac();
  ~RootWindowMac();

  BrowserWindow* browser_window() const;
  RootWindow::Delegate* delegate() const;

  // RootWindow methods.
  void Init(RootWindow::Delegate* delegate,
            const RootWindowConfig& config,
            const CefBrowserSettings& settings) OVERRIDE;
  void InitAsPopup(RootWindow::Delegate* delegate,
                   bool with_controls,
                   bool with_osr,
                   const CefPopupFeatures& popupFeatures,
                   CefWindowInfo& windowInfo,
                   CefRefPtr<CefClient>& client,
                   CefBrowserSettings& settings) OVERRIDE;
  void Show(ShowMode mode) OVERRIDE;
  void Hide() OVERRIDE;
  void SetBounds(int x, int y, size_t width, size_t height) OVERRIDE;
  void Close(bool force) OVERRIDE;
  void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  float GetDeviceScaleFactor() const OVERRIDE;
  CefRefPtr<CefBrowser> GetBrowser() const OVERRIDE;
  ClientWindowHandle GetWindowHandle() const OVERRIDE;
  bool WithWindowlessRendering() const OVERRIDE;
  bool WithExtension() const OVERRIDE;

  // BrowserWindow::Delegate methods.
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  void OnBrowserWindowDestroyed() OVERRIDE;
  void OnSetAddress(const std::string& url) OVERRIDE;
  void OnSetTitle(const std::string& title) OVERRIDE;
  void OnSetFullscreen(bool fullscreen) OVERRIDE;
  void OnAutoResize(const CefSize& new_size) OVERRIDE;
  void OnSetLoadingState(bool isLoading,
                         bool canGoBack,
                         bool canGoForward) OVERRIDE;
  void OnSetDraggableRegions(
      const std::vector<CefDraggableRegion>& regions) OVERRIDE;

  void OnNativeWindowClosed();

 private:
  CefRefPtr<RootWindowMacImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowMac);

  friend class RootWindowMacImpl;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MAC_H_
