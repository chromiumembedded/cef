// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/box_layout_impl.h"

#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/view_util.h"

// static
CefRefPtr<CefBoxLayoutImpl> CefBoxLayoutImpl::Create(
    const CefBoxLayoutSettings& settings,
    views::View* owner_view) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefBoxLayoutImpl> impl = new CefBoxLayoutImpl(settings);
  impl->Initialize(owner_view);
  return impl;
}

void CefBoxLayoutImpl::SetFlexForView(CefRefPtr<CefView> view, int flex) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK_GE(flex, 0);
  if (flex < 0)
    return;

  DCHECK(view && view->IsValid() && view->IsAttached());
  if (!view || !view->IsValid() || !view->IsAttached())
    return;

  views::View* view_ptr = view_util::GetFor(view);
  DCHECK_EQ(view_ptr->parent(), owner_view());
  if (view_ptr->parent() != owner_view())
    return;

  layout()->SetFlexForView(view_ptr, flex);
}

void CefBoxLayoutImpl::ClearFlexForView(CefRefPtr<CefView> view) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view && view->IsValid() && view->IsAttached());
  if (!view || !view->IsValid() || !view->IsAttached())
    return;

  views::View* view_ptr = view_util::GetFor(view);
  DCHECK_EQ(view_ptr->parent(), owner_view());
  if (view_ptr->parent() != owner_view())
    return;

  layout()->ClearFlexForView(view_ptr);
}

CefBoxLayoutImpl::CefBoxLayoutImpl(const CefBoxLayoutSettings& settings)
    : settings_(settings) {
}

views::BoxLayout* CefBoxLayoutImpl::CreateLayout() {
  views::BoxLayout* layout = new views::BoxLayout(
      settings_.horizontal ? views::BoxLayout::kHorizontal :
                             views::BoxLayout::kVertical,
      settings_.inside_border_horizontal_spacing,
      settings_.inside_border_vertical_spacing,
      settings_.between_child_spacing);
  layout->set_main_axis_alignment(
      static_cast<views::BoxLayout::MainAxisAlignment>(
          settings_.main_axis_alignment));
  layout->set_cross_axis_alignment(
      static_cast<views::BoxLayout::CrossAxisAlignment>(
          settings_.cross_axis_alignment));
  layout->set_inside_border_insets(gfx::Insets(
      settings_.inside_border_insets.top,
      settings_.inside_border_insets.left,
      settings_.inside_border_insets.bottom,
      settings_.inside_border_insets.right));
  layout->set_minimum_cross_axis_size(settings_.minimum_cross_axis_size);
  if (settings_.default_flex > 0)
    layout->SetDefaultFlex(settings_.default_flex);
  return layout;
}
