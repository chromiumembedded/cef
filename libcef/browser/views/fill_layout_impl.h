// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_FILL_LAYOUT_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_FILL_LAYOUT_IMPL_H_
#pragma once

#include "include/views/cef_fill_layout.h"

#include "libcef/browser/views/layout_impl.h"
#include "ui/views/layout/fill_layout.h"

class CefFillLayoutImpl :
    public CefLayoutImpl<views::FillLayout, CefFillLayout> {
 public:
  // Create a new CefFillLayout insance. |owner_view| must be non-nullptr.
  static CefRefPtr<CefFillLayout> Create(views::View* owner_view);

  // CefLayout methods:
  CefRefPtr<CefFillLayout> AsFillLayout() override { return this; }

 private:
  CefFillLayoutImpl();

  views::FillLayout* CreateLayout() override;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefFillLayoutImpl);
  DISALLOW_COPY_AND_ASSIGN(CefFillLayoutImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_FILL_LAYOUT_IMPL_H_
