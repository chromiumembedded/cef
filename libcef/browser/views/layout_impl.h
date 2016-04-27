// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_IMPL_H_
#pragma once

#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_layout.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/layout_adapter.h"
#include "libcef/browser/views/layout_util.h"

#include "base/logging.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"

// Base template for implementing CefLayout-derived classes. See comments in
// view_impl.h for a usage overview.
template <class ViewsLayoutClass, class CefLayoutClass>
class CefLayoutImpl : public CefLayoutAdapter,
                      public CefLayoutClass {
 public:
  // Returns the underlying views::LayoutManager object as the derived type.
  // Does not transfer ownership.
  ViewsLayoutClass* layout() const { return layout_ref_; }

  // Returns the views::View that owns this object.
  views::View* owner_view() const { return owner_view_; }

  // CefLayoutAdapter methods:
  views::LayoutManager* Get() const override {
    return layout();
  }
  void Detach() override {
    owner_view_ = nullptr;
    layout_ref_ = nullptr;
  }

  // CefLayout methods. When adding new As*() methods make sure to update
  // CefLayoutAdapter::GetFor() in layout_adapter.cc.
  CefRefPtr<CefBoxLayout> AsBoxLayout() override { return nullptr; }
  CefRefPtr<CefFillLayout> AsFillLayout() override { return nullptr; }
  bool IsValid() override {
    CEF_REQUIRE_UIT_RETURN(false);
    return !!layout_ref_; 
  }

 protected:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  CefLayoutImpl()
      : layout_ref_(nullptr),
        owner_view_(nullptr) {
  }

  // Initialize this object and assign ownership to |owner_view|.
  void Initialize(views::View* owner_view) {
    DCHECK(owner_view);
    owner_view_ = owner_view;
    layout_ref_ = CreateLayout();
    DCHECK(layout_ref_);
    owner_view->SetLayoutManager(layout_ref_);
    layout_util::Assign(this, owner_view);
  }

  // Create the views::LayoutManager object.
  virtual ViewsLayoutClass* CreateLayout() = 0;

 private:
  // Unowned reference to the views::LayoutManager wrapped by this object. Will
  // be nullptr after the views::LayoutManager is destroyed.
  ViewsLayoutClass* layout_ref_;

  // Unowned reference to the views::View that owns this object. Will be nullptr
  // after the views::LayoutManager is destroyed.
  views::View* owner_view_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_LAYOUT_IMPL_H_
