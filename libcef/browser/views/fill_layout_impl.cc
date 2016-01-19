// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/fill_layout_impl.h"

#include "libcef/browser/thread_util.h"

// static
CefRefPtr<CefFillLayout> CefFillLayoutImpl::Create(views::View* owner_view) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefFillLayoutImpl> impl = new CefFillLayoutImpl();
  impl->Initialize(owner_view);
  return impl;
}

CefFillLayoutImpl::CefFillLayoutImpl() {
}

views::FillLayout* CefFillLayoutImpl::CreateLayout() {
  return new views::FillLayout();
}
