// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
#pragma once

#include "include/views/cef_panel_delegate.h"

#include "libcef/browser/views/panel_view.h"

class CefBasicPanelView : public CefPanelView<views::View, CefPanelDelegate> {
 public:
  typedef CefPanelView<views::View, CefPanelDelegate> ParentClass;

  // |cef_delegate| may be nullptr.
  explicit CefBasicPanelView(CefPanelDelegate* cef_delegate);

 private:
  DISALLOW_COPY_AND_ASSIGN(CefBasicPanelView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_BASIC_PANEL_VIEW_H_
