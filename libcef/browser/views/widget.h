// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_WIDGET_H_
#define CEF_LIBCEF_BROWSER_VIEWS_WIDGET_H_
#pragma once

#include "ui/color/color_provider_key.h"

class CefWindowView;
class Profile;

namespace views {
class Widget;
}

// Interface that provides access to common CEF-specific Widget functionality.
// Alloy and Chrome styles use different views::Widget inheritance so we can't
// cast types directly. Implemented by CefWidgetImpl for Alloy style and
// ChromeBrowserFrame for Chrome style.
class CefWidget {
 public:
  // Called from CefWindowView::CreateWidget.
  static CefWidget* Create(CefWindowView* window_view);

  // Returns the CefWidget for |widget|, which must be Views-hosted.
  static CefWidget* GetForWidget(views::Widget* widget);

  // Returns the Widget runtime style.
  virtual bool IsAlloyStyle() const = 0;
  bool IsChromeStyle() const { return !IsAlloyStyle(); }

  // Returns the Widget associated with this object.
  virtual views::Widget* GetWidget() = 0;
  virtual const views::Widget* GetWidget() const = 0;

  // Called from CefWindowView::CreateWidget after Widget::Init. There will be
  // no theme-related callbacks prior to this method being called.
  virtual void Initialized() = 0;

  // Returns true if Initialize() has been called.
  virtual bool IsInitialized() const = 0;

  // Track all Profiles associated with this Widget. Called from
  // CefBrowserViewImpl::AddedToWidget and DisassociateFromWidget.
  virtual void AddAssociatedProfile(Profile* profile) = 0;
  virtual void RemoveAssociatedProfile(Profile* profile) = 0;

  // Returns the Profile that will be used for Chrome theme purposes. Chrome
  // style supports a single BrowserView in a single Widget. Alloy style
  // supports multiple BrowserViews in a single Widget, and those BrowserViews
  // may have different Profiles. If there are multiple Profiles we return an
  // arbitrary one. The returned Profile will remain consistent until the set of
  // associated Profiles changes.
  virtual Profile* GetThemeProfile() const = 0;

  // Optional special handling to toggle full-screen mode.
  virtual bool ToggleFullscreenMode() { return false; }

 protected:
  virtual ~CefWidget() = default;

  static ui::ColorProviderKey GetColorProviderKey(
      const ui::ColorProviderKey& widget_key,
      Profile* profile);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_WIDGET_H_
