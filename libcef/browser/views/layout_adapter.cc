// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/layout_adapter.h"

#include "libcef/browser/views/box_layout_impl.h"
#include "libcef/browser/views/fill_layout_impl.h"

// static
CefLayoutAdapter* CefLayoutAdapter::GetFor(CefRefPtr<CefLayout> layout) {
  CefLayoutAdapter* adapter = nullptr;
  if (layout->AsBoxLayout()) {
    adapter = static_cast<CefBoxLayoutImpl*>(layout->AsBoxLayout().get());
  } else if (layout->AsFillLayout()) {
    adapter = static_cast<CefFillLayoutImpl*>(layout->AsFillLayout().get());
  }

  DCHECK(adapter);
  return adapter;
}
