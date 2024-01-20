// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_H_
#pragma once

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"

// An overview of the Chrome Browser object model is provided below. Object
// creation normally begins with a call to Browser::Create(CreateParams) which
// then creates the necessary Browser view, window and frame objects. CEF has
// modified the default object creation model are described below to better
// integrate with the existing CEF Views APIs.
//
// OVERVIEW
//
// Browser and BrowserWindow are the primary Chrome objects. Browser provides
// the concrete state and mutation methods while BrowserWindow is an interface
// implemented by the platform-specific "view" of the Browser window.
//
// Browser:
// - Creates a BrowserView (aka BrowserWindow) and BrowserFrame (aka Widget) via
//   a call to BrowserWindow::CreateBrowserWindow() in the Browser constructor.
//   - An existing BrowserWindow can alternately be specified via the
//     Browser::CreateParams::window parameter.
// - Owned by the BrowserView after creation.
//
// The Chrome Views implementation uses BrowserView to represent the browser
// client area and BrowserFrame (plus helpers) to represent the non-client
// window frame.
//
// BrowserView:
// - Extends BrowserWindow, views::ClientView, views::WidgetDelegate.
// - Owns the Browser.
// - References the BrowserFrame.
// - Passed to Widget::Init() via Widget::InitParams::delegate to receive
//   WidgetDelegate callbacks.
// - Extended by CEF as ChromeBrowserView.
// BrowserFrame:
// - Extends Widget (aka views::internal::NativeWidgetDelegate).
// - References the BrowserView.
// - Creates/owns a DesktopBrowserFrameAura (aka NativeBrowserFrame) via
//   BrowserFrame::InitBrowserFrame().
// - Extended by CEF as ChromeBrowserFrame.
//
// Chrome custom window/frame handling is implemented using platform-specific
// objects.
//
// DesktopBrowserFrameAura:
// - Extends NativeBrowserFrame, DesktopNativeWidgetAura.
// - Acts as a helper for BrowserFrame.
// - Creates/references a BrowserDesktopWindowTreeHostWin via
//   DesktopBrowserFrameAura::InitNativeWidget().
// BrowserDesktopWindowTreeHostWin (for Windows):
// - Extends DesktopWindowTreeHost.
// - References DesktopBrowserFrameAura, BrowserView, BrowserFrame.
// - Passed to Widget::Init() via Widget::InitParams::desktop_window_tree_host.
//
// CEF MODIFICATIONS
//
// The CEF Views integration uses an alternative approach of creating the
// ChromeBrowserFrame in CefWindowView::CreateWidget() and the
// ChromeBrowserView in CefBrowserViewImpl::CreateRootView().
// The object associations described above are then configured via
// ChromeBrowserView::AddedToWidget() and ChromeBrowserHostImpl::Create()
// after the BrowserView is added to the Widget. The Chromium code has been
// patched to allow later initialization of the Browser, BrowserFrame and
// BrowserView members to support this model.
//
// CEF does not use Chrome's NativeBrowserFrame (aka DesktopBrowserFrameAura),
// BrowserNonClientFrameView or BrowserRootView objects (all normally created by
// BrowserFrame during Widget initialization). Consequently
// |BrowserFrame::native_browser_frame_| and |BrowserFrame::browser_frame_view_|
// (sometimes retrieved via BrowserFrame::GetFrameView) will be nullptr and the
// Chromium code has been patched to add the necessary null checks.
//
// CEF does not pass ChromeBrowserView as the WidgetDelegate when the Widget is
// initialized in CefWindowView::CreateWidget(). Some of the WidgetDelegate
// callbacks may need to be routed from CefWindowView to ChromeBrowserView in
// the future.
//
// See the chrome_runtime_views.patch file for the complete set of related
// modifications.

class BrowserView;

// Widget for a Views-hosted Chrome browser. Created in
// CefWindowView::CreateWidget() when the Chrome runtime is enabled.
class ChromeBrowserFrame : public BrowserFrame {
 public:
  ChromeBrowserFrame() = default;
  ChromeBrowserFrame(const ChromeBrowserFrame&) = delete;
  ChromeBrowserFrame& operator=(const ChromeBrowserFrame&) = delete;

  void Init(BrowserView* browser_view, std::unique_ptr<Browser> browser);

  void ToggleFullscreenMode();

  // views::Widget methods:
  views::internal::RootView* CreateRootView() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView()
      override;
  void Activate() override;

  BrowserView* browser_view() const { return browser_view_; }

 private:
  BrowserView* browser_view_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_FRAME_H_
