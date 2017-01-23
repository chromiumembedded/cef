// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/scroll_view_impl.h"

// static
CefRefPtr<CefScrollView> CefScrollView::CreateScrollView(
    CefRefPtr<CefViewDelegate> delegate) {
  return CefScrollViewImpl::Create(delegate);
}

// static
CefRefPtr<CefScrollViewImpl> CefScrollViewImpl::Create(
    CefRefPtr<CefViewDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  CefRefPtr<CefScrollViewImpl> view = new CefScrollViewImpl(delegate);
  view->Initialize();
  return view;
}

void CefScrollViewImpl::SetContentView(CefRefPtr<CefView> view) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  DCHECK(view.get());
  DCHECK(view->IsValid());
  DCHECK(!view->IsAttached());
  if (!view.get() || !view->IsValid() || view->IsAttached())
    return;

  std::unique_ptr<views::View> view_ptr = view_util::PassOwnership(view);
  root_view()->SetContents(view_ptr.release());
}

CefRefPtr<CefView> CefScrollViewImpl::GetContentView() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return view_util::GetFor(root_view()->contents(), false);
}

CefRect CefScrollViewImpl::GetVisibleContentRect() {
  CEF_REQUIRE_VALID_RETURN(CefRect());
  const gfx::Rect& rect = root_view()->GetVisibleRect();
  return CefRect(rect.x(), rect.y(), rect.width(), rect.height());
}

bool CefScrollViewImpl::HasHorizontalScrollbar() {
  CEF_REQUIRE_VALID_RETURN(false);
  const views::ScrollBar* scrollbar = root_view()->horizontal_scroll_bar();
  return scrollbar && scrollbar->visible();
}

int CefScrollViewImpl::GetHorizontalScrollbarHeight() {
  CEF_REQUIRE_VALID_RETURN(0);
  return root_view()->GetScrollBarLayoutHeight();
}

bool CefScrollViewImpl::HasVerticalScrollbar() {
  CEF_REQUIRE_VALID_RETURN(false);
  const views::ScrollBar* scrollbar = root_view()->vertical_scroll_bar();
  return scrollbar && scrollbar->visible();
}

int CefScrollViewImpl::GetVerticalScrollbarWidth() {
  CEF_REQUIRE_VALID_RETURN(0);
  return root_view()->GetScrollBarLayoutWidth();
}

void CefScrollViewImpl::GetDebugInfo(base::DictionaryValue* info,
                                     bool include_children) {
  ParentClass::GetDebugInfo(info, include_children);
  if (include_children) {
    views::View* view = root_view()->contents();
    CefViewAdapter* adapter = CefViewAdapter::GetFor(view);
    if (adapter) {
      std::unique_ptr<base::DictionaryValue> child_info(
          new base::DictionaryValue());
      adapter->GetDebugInfo(child_info.get(), include_children);
      info->Set("content_view", std::move(child_info));
    }
  }
}

CefScrollViewImpl::CefScrollViewImpl(CefRefPtr<CefViewDelegate> delegate)
    : ParentClass(delegate) {
}

CefScrollViewView* CefScrollViewImpl::CreateRootView() {
  return new CefScrollViewView(delegate());
}

void CefScrollViewImpl::InitializeRootView() {
  static_cast<CefScrollViewView*>(root_view())->Initialize();
}
