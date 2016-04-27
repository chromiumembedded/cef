// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_PANEL_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_PANEL_IMPL_H_
#pragma once

#include "include/views/cef_fill_layout.h"
#include "include/views/cef_layout.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_window.h"

#include "libcef/browser/views/box_layout_impl.h"
#include "libcef/browser/views/fill_layout_impl.h"
#include "libcef/browser/views/layout_util.h"
#include "libcef/browser/views/view_impl.h"

#include "base/logging.h"

// Helpers for template boiler-plate.
#define CEF_PANEL_IMPL_T CEF_VIEW_IMPL_T
#define CEF_PANEL_IMPL_A CEF_VIEW_IMPL_A
#define CEF_PANEL_IMPL_D CefPanelImpl<CEF_PANEL_IMPL_A>

// Template for implementing CefPanel-derived classes. See comments in
// view_impl.h for a usage overview.
CEF_PANEL_IMPL_T class CefPanelImpl : public CEF_VIEW_IMPL_D {
 public:
  typedef CEF_VIEW_IMPL_D ParentClass;

  // CefPanel methods. When adding new As*() methods make sure to update
  // CefViewAdapter::GetFor() in view_adapter.cc.
  CefRefPtr<CefWindow> AsWindow() override { return nullptr; }
  CefRefPtr<CefFillLayout> SetToFillLayout() override;
  CefRefPtr<CefBoxLayout> SetToBoxLayout(
      const CefBoxLayoutSettings& settings) override;
  CefRefPtr<CefLayout> GetLayout() override;
  void Layout() override;
  void AddChildView(CefRefPtr<CefView> view) override;
  void AddChildViewAt(CefRefPtr<CefView> view,
                      int index) override;
  void ReorderChildView(CefRefPtr<CefView> view,
                        int index) override;
  void RemoveChildView(CefRefPtr<CefView> view) override;
  void RemoveAllChildViews() override;
  size_t GetChildViewCount() override;
  CefRefPtr<CefView> GetChildViewAt(int index) override;

  // CefView methods:
  CefRefPtr<CefPanel> AsPanel() override { return this; }

  // CefViewAdapter methods:
  void GetDebugInfo(base::DictionaryValue* info,
                    bool include_children) override {
    ParentClass::GetDebugInfo(info, include_children);
    if (include_children) {
      const size_t count = ParentClass::content_view()->child_count();
      if (count > 0U) {
        std::unique_ptr<base::ListValue> children(new base::ListValue());

        for (size_t i = 0U; i < count; ++i) {
          views::View* view = ParentClass::content_view()->child_at(i);
          CefViewAdapter* adapter = CefViewAdapter::GetFor(view);
          if (adapter) {
            std::unique_ptr<base::DictionaryValue> child_info(
                new base::DictionaryValue());
            adapter->GetDebugInfo(child_info.get(), include_children);
            children->Append(std::move(child_info));
          }
        }

        info->Set("children", std::move(children));
      }
    }
  }

 protected:
  // Create a new implementation object.
  // Always call Initialize() after creation.
  // |delegate| may be nullptr.
  explicit CefPanelImpl(CefRefPtr<CefViewDelegateClass> delegate)
      : ParentClass(delegate) {
  }

  void Initialize() override {
    ParentClass::Initialize();

    // Create the default layout object.
    SetToFillLayout();
  }
};

CEF_PANEL_IMPL_T CefRefPtr<CefFillLayout> CEF_PANEL_IMPL_D::SetToFillLayout() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return CefFillLayoutImpl::Create(ParentClass::content_view());
}

CEF_PANEL_IMPL_T CefRefPtr<CefBoxLayout> CEF_PANEL_IMPL_D::SetToBoxLayout(
    const CefBoxLayoutSettings& settings) {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return CefBoxLayoutImpl::Create(settings, ParentClass::content_view());
}

CEF_PANEL_IMPL_T CefRefPtr<CefLayout> CEF_PANEL_IMPL_D::GetLayout() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return layout_util::GetFor(ParentClass::content_view());
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::Layout() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  return ParentClass::root_view()->Layout();
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::AddChildView(
    CefRefPtr<CefView> view) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view.get());
  DCHECK(view->IsValid());
  DCHECK(!view->IsAttached());
  if (!view.get() || !view->IsValid() || view->IsAttached())
    return;

  std::unique_ptr<views::View> view_ptr = view_util::PassOwnership(view);
  ParentClass::content_view()->AddChildView(view_ptr.release());
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::AddChildViewAt(
    CefRefPtr<CefView> view,
    int index) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view.get());
  DCHECK(view->IsValid());
  DCHECK(!view->IsAttached());
  DCHECK_GE(index, 0);
  DCHECK_LE(index, ParentClass::content_view()->child_count());
  if (!view.get() || !view->IsValid() || view->IsAttached() ||
      index < 0 || index > ParentClass::content_view()->child_count()) {
    return;
  }

  std::unique_ptr<views::View> view_ptr = view_util::PassOwnership(view);
  ParentClass::content_view()->AddChildViewAt(view_ptr.release(), index);
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::ReorderChildView(
    CefRefPtr<CefView> view,
    int index) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view.get());
  DCHECK(view->IsValid());
  DCHECK(view->IsAttached());
  if (!view.get() || !view->IsValid() || !view->IsAttached())
    return;
  
  views::View* view_ptr = view_util::GetFor(view);
  DCHECK(view_ptr);
  DCHECK_EQ(view_ptr->parent(), ParentClass::content_view());
  if (!view_ptr || view_ptr->parent() != ParentClass::content_view())
    return;

  ParentClass::content_view()->ReorderChildView(view_ptr, index);
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::RemoveChildView(
    CefRefPtr<CefView> view) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view.get());
  DCHECK(view->IsValid());
  DCHECK(view->IsAttached());
  if (!view.get() || !view->IsValid() || !view->IsAttached())
    return;

  views::View* view_ptr = view_util::GetFor(view);
  DCHECK(view_ptr);
  DCHECK_EQ(view_ptr->parent(), ParentClass::content_view());
  if (!view_ptr || view_ptr->parent() != ParentClass::content_view())
    return;

  ParentClass::content_view()->RemoveChildView(view_ptr);
  view_util::ResumeOwnership(view);
}

CEF_PANEL_IMPL_T void CEF_PANEL_IMPL_D::RemoveAllChildViews() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  while (ParentClass::content_view()->has_children()) {
    CefRefPtr<CefView> view =
        view_util::GetFor(ParentClass::content_view()->child_at(0), false);
    RemoveChildView(view);
  }
}

CEF_PANEL_IMPL_T size_t CEF_PANEL_IMPL_D::GetChildViewCount() {
  CEF_REQUIRE_VALID_RETURN(0U);
  return ParentClass::content_view()->child_count();
}

CEF_PANEL_IMPL_T CefRefPtr<CefView> CEF_PANEL_IMPL_D::GetChildViewAt(
    int index) {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, ParentClass::content_view()->child_count());
  if (index < 0 || index >= ParentClass::content_view()->child_count())
    return nullptr;

  CefRefPtr<CefView> view =
      view_util::GetFor(ParentClass::content_view()->child_at(index), false);
  DCHECK(view);
  return view;
}

#endif  // CEF_LIBCEF_BROWSER_VIEWS_PANEL_IMPL_H_
