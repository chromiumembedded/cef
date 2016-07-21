// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/basic_panel_impl.h"

#include "libcef/browser/views/basic_panel_view.h"

// static
CefRefPtr<CefPanel> CefPanel::CreatePanel(
    CefRefPtr<CefPanelDelegate> delegate) {
  return CefBasicPanelImpl::Create(delegate);
}

// static
CefRefPtr<CefBasicPanelImpl> CefBasicPanelImpl::Create(
    CefRefPtr<CefPanelDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefBasicPanelImpl> panel = new CefBasicPanelImpl(delegate);
  panel->Initialize();
  return panel;
}

CefBasicPanelImpl::CefBasicPanelImpl(CefRefPtr<CefPanelDelegate> delegate)
    : ParentClass(delegate) {
}

views::View* CefBasicPanelImpl::CreateRootView() {
  return new CefBasicPanelView(delegate());
}

void CefBasicPanelImpl::InitializeRootView() {
  static_cast<CefBasicPanelView*>(root_view())->Initialize();
}
