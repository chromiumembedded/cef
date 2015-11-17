// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/native/window_delegate_view.h"

#include "content/public/browser/web_contents.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

CefWindowDelegateView::CefWindowDelegateView(SkColor background_color)
  : background_color_(background_color),
    web_view_(NULL) {
}

void CefWindowDelegateView::Init(
    gfx::AcceleratedWidget parent_widget,
    content::WebContents* web_contents,
    const gfx::Rect& bounds) {
  DCHECK(!web_view_);
  web_view_ = new views::WebView(web_contents->GetBrowserContext());
  web_view_->SetWebContents(web_contents);
  web_view_->SetPreferredSize(bounds.size());

  views::Widget* widget = new views::Widget;

  // See CalculateWindowStylesFromInitParams in
  // ui/views/widget/widget_hwnd_utils.cc for the conversion of |params| to
  // Windows style flags.
  views::Widget::InitParams params;
  params.parent_widget = parent_widget;
  params.bounds = bounds;
  params.delegate = this;
  // Set the WS_CHILD flag.
  params.child = true;
  // Set the WS_VISIBLE flag.
  params.type = views::Widget::InitParams::TYPE_CONTROL;
  // Don't set the WS_EX_COMPOSITED flag.
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  // Tell Aura not to draw the window frame on resize.
  params.remove_standard_frame = true;
  // Cause WidgetDelegate::CanActivate to return true. See comments in
  // CefBrowserHostImpl::PlatformSetFocus.
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;

  // Results in a call to InitContent().
  widget->Init(params);

  // |widget| should now be associated with |this|.
  DCHECK_EQ(widget, GetWidget());
  // |widget| must be top-level for focus handling to work correctly.
  DCHECK(widget->is_top_level());
  // |widget| must be activatable for focus handling to work correctly.
  DCHECK(widget->widget_delegate()->CanActivate());
}

void CefWindowDelegateView::InitContent() {
  set_background(views::Background::CreateSolidBackground(background_color_));
  SetLayoutManager(new views::FillLayout());
  AddChildView(web_view_);
}

void CefWindowDelegateView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    InitContent();
}

