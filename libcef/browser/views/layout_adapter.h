// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_ADAPTER_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_ADAPTER_H_
#pragma once

#include "include/views/cef_layout.h"

namespace views {
class LayoutManager;
};

// Exposes a common interface from all CefLayout implementation objects to
// simplify the layout_util implementation. See comments in view_impl.h for a
// usage overview.
class CefLayoutAdapter {
 public:
  CefLayoutAdapter() {}

  // Returns the CefLayoutAdapter for the specified |layout|.
  static CefLayoutAdapter* GetFor(CefRefPtr<CefLayout> layout);

  // Returns the underlying views::LayoutManager object. Does not transfer
  // ownership.
  virtual views::LayoutManager* Get() const = 0;

  // Release all references to the views::LayoutManager object. This is called
  // when the views::LayoutManager is deleted after being assigned to a
  // views::View.
  virtual void Detach() = 0;

 protected:
  virtual ~CefLayoutAdapter() {}
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_ADAPTER_H_
