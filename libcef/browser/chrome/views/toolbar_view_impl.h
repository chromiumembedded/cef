// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_TOOLBAR_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_TOOLBAR_VIEW_IMPL_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/views/cef_view_delegate.h"
#include "cef/libcef/browser/chrome/views/toolbar_view_view.h"
#include "cef/libcef/browser/views/view_impl.h"

class Browser;
class BrowserView;

class CefToolbarViewImpl
    : public CefViewImpl<CefToolbarViewView, CefView, CefViewDelegate> {
 public:
  using ParentClass = CefViewImpl<CefToolbarViewView, CefView, CefViewDelegate>;

  CefToolbarViewImpl(const CefToolbarViewImpl&) = delete;
  CefToolbarViewImpl& operator=(const CefToolbarViewImpl&) = delete;

  void Destroyed();

  // Create a new CefToolbarViewImpl instance. |delegate| may be nullptr.
  static CefRefPtr<CefToolbarViewImpl> Create(
      CefRefPtr<CefViewDelegate> delegate,
      Browser* browser,
      BrowserView* browser_view,
      std::optional<ToolbarView::DisplayMode> display_mode);

  static const char* const kTypeString;

  // CefViewAdapter methods:
  std::string GetDebugType() override { return kTypeString; }

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  CefToolbarViewImpl(CefRefPtr<CefViewDelegate> delegate,
                     Browser* browser,
                     BrowserView* browser_view,
                     std::optional<ToolbarView::DisplayMode> display_mode);

  // CefViewImpl methods:
  CefToolbarViewView* CreateRootView() override;
  void InitializeRootView() override;

  raw_ptr<Browser> browser_;
  raw_ptr<BrowserView> browser_view_;
  std::optional<ToolbarView::DisplayMode> const display_mode_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefToolbarViewImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_IMPL_H_
