// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/browser_view_view.h"

#include "libcef/browser/views/browser_view_impl.h"

CefBrowserViewView::CefBrowserViewView(CefBrowserViewDelegate* cef_delegate,
                                       Delegate* browser_view_delegate)
    : ParentClass(cef_delegate),
      browser_view_delegate_(browser_view_delegate) {
  DCHECK(browser_view_delegate_);
}

void CefBrowserViewView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
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
      if (!size.IsEmpty())
        SetSize(size);
    }

    browser_view_delegate_->OnBrowserViewAdded();
  }
}
