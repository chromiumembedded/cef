// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/native/native_widget_delegate.h"

#include <utility>

#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

// Owned by the Widget, via CefNativeWidgetDelegate.
class CefNativeContentsView : public views::View {
  METADATA_HEADER(CefNativeContentsView, views::View)

 public:
  explicit CefNativeContentsView(CefNativeWidgetDelegate* window_delegate)
      : window_delegate_(window_delegate) {}

  CefNativeContentsView(const CefNativeContentsView&) = delete;
  CefNativeContentsView& operator=(const CefNativeContentsView&) = delete;

 private:
  // View methods:
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this) {
      SetBackground(
          views::CreateSolidBackground(window_delegate_->background_color_));
      SetLayoutManager(std::make_unique<views::FillLayout>());
      AddChildView(window_delegate_->web_view_.get());
    }
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    views::View::OnBoundsChanged(previous_bounds);
    if (!window_delegate_->on_bounds_changed_.is_null()) {
      window_delegate_->on_bounds_changed_.Run();
    }
  }

 private:
  raw_ptr<CefNativeWidgetDelegate> window_delegate_;
};

BEGIN_METADATA(CefNativeContentsView)
END_METADATA

CefNativeWidgetDelegate::CefNativeWidgetDelegate(
    SkColor background_color,
    bool always_on_top,
    base::RepeatingClosure on_bounds_changed,
    base::OnceClosure on_delete)
    : background_color_(background_color),
      always_on_top_(always_on_top),
      on_bounds_changed_(std::move(on_bounds_changed)),
      on_delete_(std::move(on_delete)) {}

void CefNativeWidgetDelegate::Init(gfx::AcceleratedWidget parent_widget,
                                   content::WebContents* web_contents,
                                   const gfx::Rect& bounds) {
  DCHECK(!contents_view_);
  contents_view_ = new CefNativeContentsView(this);

  DCHECK(!web_view_);
  web_view_ = new views::WebView(web_contents->GetBrowserContext());
  web_view_->SetWebContents(web_contents);
  web_view_->SetPreferredSize(bounds.size());

  SetCanResize(true);

  widget_ = std::make_unique<views::Widget>();

  // See CalculateWindowStylesFromInitParams in
  // ui/views/widget/widget_hwnd_utils.cc for the conversion of |params| to
  // Windows style flags.
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.parent_widget = parent_widget;
  params.bounds = bounds;
  params.delegate = this;
  // Set the WS_CHILD flag.
  params.child = true;
  // Set the WS_VISIBLE flag.
  params.type = views::Widget::InitParams::TYPE_CONTROL;
  // Don't set the WS_EX_COMPOSITED flag.
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  // Tell Aura not to draw the window frame on resize.
  params.remove_standard_frame = true;
  // Cause WidgetDelegate::CanActivate to return true. See comments in
  // AlloyBrowserHostImpl::PlatformSetFocus.
  params.activatable = views::Widget::InitParams::Activatable::kYes;

  params.z_order = always_on_top_ ? ui::ZOrderLevel::kFloatingWindow
                                  : ui::ZOrderLevel::kNormal;

  // Results in a call to InitContent().
  widget_->Init(std::move(params));

  // |widget| should now be associated with |this|.
  DCHECK_EQ(widget_.get(), GetWidget());
  // |widget| must be top-level for focus handling to work correctly.
  DCHECK(widget_->is_top_level());
  // |widget| must be activatable for focus handling to work correctly.
  DCHECK(widget_->widget_delegate()->CanActivate());
}

void CefNativeWidgetDelegate::WidgetIsZombie(views::Widget* widget) {
  contents_view_ = nullptr;
  web_view_ = nullptr;

  // This triggers deletion of contained Views.
  widget_.reset();

  if (!on_delete_.is_null()) {
    std::move(on_delete_).Run();
  }

  delete this;
}
