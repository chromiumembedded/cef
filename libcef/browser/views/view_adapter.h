// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_VIEW_ADAPTER_H_
#define CEF_LIBCEF_BROWSER_VIEWS_VIEW_ADAPTER_H_
#pragma once

#include "include/views/cef_view.h"

namespace base {
class DictionaryValue;
}

namespace views {
class View;
}

// Exposes a common interface from all CefView implementation objects to
// simplify the view_util implementation. See comments in view_impl.h for a
// usage overview.
class CefViewAdapter {
 public:
  CefViewAdapter() {}

  // Returns the CefViewAdapter for the specified |view|.
  static CefViewAdapter* GetFor(CefRefPtr<CefView> view);
  static CefViewAdapter* GetFor(views::View* view);

  // Returns the underlying views::View object. Does not transfer ownership.
  virtual views::View* Get() const = 0;

  // Pass ownership of the underlying views::View object to the caller. This
  // object keeps an unowned reference to the views::View object. This is called
  // when the views::View is parented to another views::View.
  virtual std::unique_ptr<views::View> PassOwnership() = 0;

  // Resume ownership of the underlying views::View object. This is called when
  // the views::View is no longer parented to another views::View.
  virtual void ResumeOwnership() = 0;

  // Release all references to the views::View object. This is called when the
  // views::View is deleted after being parented to another views::View.
  virtual void Detach() = 0;

  // Override this method to provide a string representation of the View type.
  // Only implement this method in concrete classes.
  virtual std::string GetDebugType() = 0;

  // Override this method to provide debug info specific to the View type.
  virtual void GetDebugInfo(base::DictionaryValue* info,
                            bool include_children) = 0;

 protected:
  virtual ~CefViewAdapter() {}
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_VIEW_ADAPTER_H_
