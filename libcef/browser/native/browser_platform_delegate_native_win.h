// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_WIN_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_WIN_H_

#include <windows.h>

#include "libcef/browser/native/browser_platform_delegate_native_aura.h"

// Windowed browser implementation for Windows.
class CefBrowserPlatformDelegateNativeWin
    : public CefBrowserPlatformDelegateNativeAura {
 public:
  CefBrowserPlatformDelegateNativeWin(const CefWindowInfo& window_info,
                                      SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void BrowserDestroyed(CefBrowserHostBase* browser) override;
  bool CreateHostWindow() override;
  void CloseHostWindow() override;
  CefWindowHandle GetHostWindowHandle() const override;
  views::Widget* GetWindowWidget() const override;
  void SetFocus(bool setFocus) override;
  void NotifyMoveOrResizeStarted() override;
  void SizeTo(int width, int height) override;
  gfx::Point GetScreenPoint(const gfx::Point& view) const override;
  void ViewText(const std::string& text) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  CefEventHandle GetEventHandle(
      const content::NativeWebKeyboardEvent& event) const override;
  std::unique_ptr<CefFileDialogRunner> CreateFileDialogRunner() override;
  std::unique_ptr<CefJavaScriptDialogRunner> CreateJavaScriptDialogRunner()
      override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;

  // CefBrowserPlatformDelegateNativeAura methods:
  ui::KeyEvent TranslateUiKeyEvent(const CefKeyEvent& key_event) const override;
  gfx::Vector2d GetUiWheelEventOffset(int deltaX, int deltaY) const override;

 private:
  static void RegisterWindowClass();
  static LPCTSTR GetWndClass();
  static LRESULT CALLBACK WndProc(HWND hwnd,
                                  UINT message,
                                  WPARAM wParam,
                                  LPARAM lParam);

  // True if the host window has been created.
  bool host_window_created_ = false;

  // Widget hosting the web contents. It will be deleted automatically when the
  // associated root window is destroyed.
  views::Widget* window_widget_ = nullptr;

  bool has_frame_ = false;
  bool called_enable_non_client_dpi_scaling_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_WIN_H_
