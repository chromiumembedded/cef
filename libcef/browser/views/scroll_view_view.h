// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_VIEW_H_
#pragma once

#include "cef/include/views/cef_panel_delegate.h"
#include "cef/libcef/browser/views/view_view.h"
#include "ui/views/controls/scroll_view.h"

class CefScrollViewView
    : public CefViewView<views::ScrollView, CefViewDelegate> {
  METADATA_HEADER(CefScrollViewView, views::ScrollView)

 public:
  using ParentClass = CefViewView<views::ScrollView, CefViewDelegate>;

  // |cef_delegate| may be nullptr.
  explicit CefScrollViewView(CefViewDelegate* cef_delegate);

  CefScrollViewView(const CefScrollViewView&) = delete;
  CefScrollViewView& operator=(const CefScrollViewView&) = delete;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_SCROLL_VIEW_VIEW_H_
