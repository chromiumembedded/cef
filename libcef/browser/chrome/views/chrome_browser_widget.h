// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_WIDGET_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_WIDGET_H_
#pragma once

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cef/libcef/browser/views/color_provider_tracker.h"
#include "cef/libcef/browser/views/widget.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"

// An overview of the Chrome Browser object model is provided below. Object
// creation normally begins with a call to Browser::Create(CreateParams) which
// then creates the necessary Browser view, window and widget objects. CEF has
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
// - Creates a BrowserView (aka BrowserWindow) and BrowserWidget (aka Widget)
//   via a call to BrowserWindow::CreateBrowserWindow() in the Browser
//   constructor.
//   - An existing BrowserWindow can alternately be specified via the
//     Browser::CreateParams::window parameter.
// - Owned by BrowserManagerService with destruction triggered via
//   Browser::OnWindowClosing.
// - Owns the BrowserView (as BrowserWindow) and triggers BrowserWidget +
//   BrowserView destruction via BrowserView::DeleteBrowserWindow.
//
// The Chrome Views implementation uses BrowserView to represent the browser
// client area and BrowserWidget (plus helpers) to represent the non-client
// window frame.
//
// BrowserView:
// - Extends BrowserWindow, views::ClientView, views::WidgetDelegate.
// - Owned by the BrowserWidget via the views hierarchy.
// - Owns the BrowserWidget (see above DeleteBrowserWindow comments).
// - Passed to Widget::Init() via Widget::InitParams::delegate to receive
//   WidgetDelegate callbacks.
// - Extended by CEF as ChromeBrowserView.
//
// BrowserWidget:
// - Extends Widget (aka views::internal::NativeWidgetDelegate).
// - Owned by the BrowserView (see above DeleteBrowserWindow comments).
// - Owns the BrowserView via the views hierarchy.
// - Creates/owns a DesktopBrowserWidgetAura (aka NativeBrowserWidget) via
//   BrowserWidget::InitBrowserWidget().
// - Extended by CEF as ChromeBrowserWidget.
//
// Chrome custom window/frame handling is implemented using platform-specific
// objects.
//
// DesktopBrowserWidgetAura:
// - Extends NativeBrowserWidget, DesktopNativeWidgetAura.
// - Acts as a helper for BrowserWidget.
// - Creates/references a BrowserDesktopWindowTreeHostWin via
//   DesktopBrowserWidgetAura::InitNativeWidget().
// BrowserDesktopWindowTreeHostWin (for Windows):
// - Extends DesktopWindowTreeHost.
// - References DesktopBrowserWidgetAura, BrowserView, BrowserWidget.
// - Passed to Widget::Init() via Widget::InitParams::desktop_window_tree_host.
//
// CEF MODIFICATIONS
//
// The CEF Views integration uses an alternative approach of creating the
// ChromeBrowserWidget in CefWindowView::CreateWidget() and the
// ChromeBrowserView in CefBrowserViewImpl::CreateRootView().
// The object associations described above are then configured via
// ChromeBrowserView::AddedToWidget() and ChromeBrowserHostImpl::CreateBrowser()
// after the BrowserView is added to the Widget. The Chromium code has been
// patched to allow later initialization of the Browser, BrowserWidget and
// BrowserView members to support this model.
//
// CEF bypasses the default BrowserWidget destruction in
// BrowserView::DeleteBrowserWindow (usually triggered by Browser destruction).
// Instead, CefWindowWidgetDelegate owns the ChromeBrowserWidget and destroys
// both the ChromeBrowserWidget and itself in WidgetIsZombie (last callback
// during native widget destruction). This triggers Browser destuction via
// direct ChromeBrowserWidget ownership and BrowserView destruction via the
// views hierarchy (see ChromeBrowserWidget::OnNativeWidgetDestroyed).
//
// CEF does not use Chrome's BrowserNativeWidget (aka DesktopNativeWidgetAura),
// BrowserFrameView or BrowserRootView objects (all normally created by
// BrowserWidget during Widget initialization). Instead, all objects are the
// Widget defaults (e.g. Widget::CreateFrameView, Widget::CreateRootView), and
// ChromeBrowserFrameView is provided as a stub implementation of
// BrowserFrameView to satisfy minimal usage expectations via
// BrowserWidget::GetFrameView and similar.
//
// CEF does not pass ChromeBrowserView as the WidgetDelegate when the Widget is
// initialized in CefWindowView::CreateWidget(). Some of the WidgetDelegate
// callbacks may need to be routed from CefWindowView to ChromeBrowserView in
// the future.
//
// See the chrome_runtime_views.patch file for the complete set of related
// modifications.

class BrowserView;
class CefWindowView;

// Widget for a Views-hosted Chrome browser. Created in
// CefWindowView::CreateWidget() with Chrome style.
class ChromeBrowserWidget : public BrowserWidget,
                            public CefWidget,
                            public CefColorProviderTracker::Observer,
                            public ThemeServiceObserver {
 public:
  explicit ChromeBrowserWidget(CefWindowView* window_view);
  ~ChromeBrowserWidget() override;

  ChromeBrowserWidget(const ChromeBrowserWidget&) = delete;
  ChromeBrowserWidget& operator=(const ChromeBrowserWidget&) = delete;

  // Called from ChromeBrowserView::InitBrowser after |browser| creation.
  void Init(BrowserView* browser_view, Browser* browser);

  // CefWidget methods:
  bool IsAlloyStyle() const override { return false; }
  views::Widget* GetWidget() override { return this; }
  const views::Widget* GetWidget() const override { return this; }
  void Initialized() override;
  bool IsInitialized() const override { return initialized_; }
  void AddAssociatedProfile(Profile* profile) override;
  void RemoveAssociatedProfile(Profile* profile) override;
  Profile* GetThemeProfile() const override;
  bool ToggleFullscreenMode() override;

  // BrowserWidget methods:
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override;

  // views::Widget methods:
  views::internal::RootView* CreateRootView() override;
  std::unique_ptr<views::FrameView> CreateFrameView() override;
  void Activate() override;

  // NativeWidgetDelegate methods:
  void OnNativeWidgetDestroyed() override;

  // ui::NativeThemeObserver methods:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
  ui::ColorProviderKey GetColorProviderKey() const override;

  // ThemeServiceObserver methods:
  void OnThemeChanged() override;

 private:
  // CefColorProviderTracker::Observer methods:
  void OnColorProviderCacheResetMissed() override;

  void NotifyThemeColorsChanged(bool chrome_theme);

  raw_ptr<CefWindowView> window_view_;
  std::unique_ptr<BrowserFrameView> frame_view_;

  bool initialized_ = false;
  bool native_theme_change_ = false;

  // Map of Profile* to count.
  using ProfileMap = std::map<raw_ptr<Profile>, size_t>;
  ProfileMap associated_profiles_;

  CefColorProviderTracker color_provider_tracker_{this};

  base::WeakPtrFactory<ChromeBrowserWidget> weak_ptr_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_BROWSER_WIDGET_H_
