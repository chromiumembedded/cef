// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
#pragma once

#include "cef/include/views/cef_panel_delegate.h"
#include "cef/libcef/browser/views/panel_view.h"

class CefBasicPanelView : public CefPanelView<views::View, CefPanelDelegate> {
  METADATA_HEADER(CefBasicPanelView, views::View)

 public:
  using ParentClass = CefPanelView<views::View, CefPanelDelegate>;

  // |cef_delegate| may be nullptr.
  explicit CefBasicPanelView(CefPanelDelegate* cef_delegate);

  CefBasicPanelView(const CefBasicPanelView&) = delete;
  CefBasicPanelView& operator=(const CefBasicPanelView&) = delete;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
