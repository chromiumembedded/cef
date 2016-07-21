// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_IMPL_H_
#pragma once

#include "include/views/cef_scroll_view.h"
#include "include/views/cef_view_delegate.h"

#include "libcef/browser/views/scroll_view_view.h"
#include "libcef/browser/views/view_impl.h"

class CefScrollViewImpl :
    public CefViewImpl<CefScrollViewView, CefScrollView, CefViewDelegate> {
 public:
  typedef CefViewImpl<CefScrollViewView, CefScrollView, CefViewDelegate>
      ParentClass;

  // Create a new CefScrollView instance. |delegate| may be nullptr.
  static CefRefPtr<CefScrollViewImpl> Create(
      CefRefPtr<CefViewDelegate> delegate);

  // CefScrollView methods:
  CefRefPtr<CefScrollView> AsScrollView() override { return this; }
  void SetContentView(CefRefPtr<CefView> view) override;
  CefRefPtr<CefView> GetContentView() override;
  CefRect GetVisibleContentRect() override;
  bool HasHorizontalScrollbar() override;
  int GetHorizontalScrollbarHeight() override;
  bool HasVerticalScrollbar() override;
  int GetVerticalScrollbarWidth() override;

  // CefViewAdapter methods:
  std::string GetDebugType() override { return "ScrollView"; }
  void GetDebugInfo(base::DictionaryValue* info,
                    bool include_children) override;

 private:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  CefScrollViewImpl(CefRefPtr<CefViewDelegate> delegate);

  // CefViewImpl methods:
  CefScrollViewView* CreateRootView() override;
  void InitializeRootView() override;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefScrollViewImpl);
  DISALLOW_COPY_AND_ASSIGN(CefScrollViewImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_IMPL_H_
