// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/browser_view_view.h"

#include "cef/libcef/browser/views/browser_view_impl.h"
#include "ui/base/metadata/metadata_impl_macros.h"

CefBrowserViewView::CefBrowserViewView(CefBrowserViewDelegate* cef_delegate,
                                       Delegate* browser_view_delegate)
    : ParentClass(cef_delegate), browser_view_delegate_(browser_view_delegate) {
  DCHECK(browser_view_delegate_);
}

void CefBrowserViewView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  ParentClass::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this) {
    gfx::Size size = GetPreferredSize();
    if (size.IsEmpty()) {
      // No size was provided for this View. Size it to the parent by default
      // or, depending on the Layout, the browser may be initially 0x0 size and
      // will not display until the parent is next resized (resulting in a call
      // to WebView::OnBoundsChanged). For example, this can happen when adding
      // this View to a CefWindow with FillLayout and then calling
      // CefWindow::Show() without first resizing the CefWindow.
      size = details.parent->GetPreferredSize();
      if (!size.IsEmpty()) {
        SetSize(size);
      }
    }
  }
}

void CefBrowserViewView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  ParentClass::OnBoundsChanged(previous_bounds);
  browser_view_delegate_->OnBoundsChanged();
}

void CefBrowserViewView::OnGestureEvent(ui::GestureEvent* event) {
  if (browser_view_delegate_->OnGestureEvent(event)) {
    return;
  }
  ParentClass::OnGestureEvent(event);
}

void CefBrowserViewView::AddedToWidget() {
  ParentClass::AddedToWidget();
  browser_view_delegate_->AddedToWidget();
}

void CefBrowserViewView::RemovedFromWidget() {
  ParentClass::RemovedFromWidget();
  browser_view_delegate_->RemovedFromWidget();
}

BEGIN_METADATA(WebViewEx)
END_METADATA

BEGIN_METADATA(CefBrowserViewView)
END_METADATA
