// Copyright 2021 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "cef/libcef/browser/views/overlay_view_host.h"

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/views/view_util.h"
#include "cef/libcef/browser/views/window_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/theme_copying_widget.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"

namespace {

class CefOverlayControllerImpl : public CefOverlayController {
 public:
  CefOverlayControllerImpl(CefOverlayViewHost* host, CefRefPtr<CefView> view)
      : host_(host), view_(view) {}

  CefOverlayControllerImpl(const CefOverlayControllerImpl&) = delete;
  CefOverlayControllerImpl& operator=(const CefOverlayControllerImpl&) = delete;

  bool IsValid() override {
    // View validity implies that CefOverlayViewHost is still valid, because the
    // Widget that it owns (and that owns the View) is still valid.
    return view_ && view_->IsValid();
  }

  bool IsSame(CefRefPtr<CefOverlayController> that) override {
    return IsValid() && that && that->IsValid() &&
           that->GetContentsView()->IsSame(view_);
  }

  CefRefPtr<CefView> GetContentsView() override { return view_; }

  CefRefPtr<CefWindow> GetWindow() override {
    if (IsValid()) {
      return view_util::GetWindowFor(host_->window_view()->GetWidget());
    }
    return nullptr;
  }

  cef_docking_mode_t GetDockingMode() override {
    if (IsValid()) {
      return host_->docking_mode();
    }
    return CEF_DOCKING_MODE_TOP_LEFT;
  }

  void Destroy() override {
    if (IsValid()) {
      // Results in a call to Destroyed().
      host_->Close();
    }
  }

  void Destroyed() {
    DCHECK(view_);
    view_ = nullptr;
    host_ = nullptr;
  }

  void SetBounds(const CefRect& bounds) override {
    if (IsValid() && host_->docking_mode() == CEF_DOCKING_MODE_CUSTOM) {
      host_->SetOverlayBounds(
          gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height));
    }
  }

  CefRect GetBounds() override {
    if (IsValid()) {
      const auto& bounds = host_->bounds();
      return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
    }
    return CefRect();
  }

  CefRect GetBoundsInScreen() override {
    if (IsValid()) {
      const auto& bounds = host_->widget()->GetWindowBoundsInScreen();
      return CefRect(bounds.x(), bounds.y(), bounds.width(), bounds.height());
    }
    return CefRect();
  }

  void SetSize(const CefSize& size) override {
    if (IsValid() && host_->docking_mode() == CEF_DOCKING_MODE_CUSTOM) {
      // Update the size without changing the origin.
      const auto& origin = host_->bounds().origin();
      host_->SetOverlayBounds(
          gfx::Rect(origin, gfx::Size(size.width, size.height)));
    }
  }

  CefSize GetSize() override {
    const auto& bounds = GetBounds();
    return CefSize(bounds.width, bounds.height);
  }

  void SetPosition(const CefPoint& position) override {
    if (IsValid() && host_->docking_mode() == CEF_DOCKING_MODE_CUSTOM) {
      // Update the origin without changing the size.
      const auto& size = host_->bounds().size();
      host_->SetOverlayBounds(
          gfx::Rect(gfx::Point(position.x, position.y), size));
    }
  }

  CefPoint GetPosition() override {
    const auto& bounds = GetBounds();
    return CefPoint(bounds.x, bounds.y);
  }

  void SetInsets(const CefInsets& insets) override {
    if (IsValid() && host_->docking_mode() != CEF_DOCKING_MODE_CUSTOM) {
      host_->SetOverlayInsets(insets);
    }
  }

  CefInsets GetInsets() override {
    if (IsValid()) {
      return host_->insets();
    }
    return CefInsets();
  }

  void SizeToPreferredSize() override {
    if (IsValid()) {
      if (host_->docking_mode() == CEF_DOCKING_MODE_CUSTOM) {
        // Update the size without changing the origin.
        const auto& origin = host_->bounds().origin();
        const auto& preferred_size = host_->view()->GetPreferredSize();
        host_->SetOverlayBounds(gfx::Rect(origin, preferred_size));
      } else {
        host_->MoveIfNecessary();
      }
    }
  }

  void SetVisible(bool visible) override {
    if (IsValid()) {
      if (visible) {
        host_->MoveIfNecessary();
        host_->widget()->Show();
      } else {
        host_->widget()->Hide();
      }
    }
  }

  bool IsVisible() override {
    if (IsValid()) {
      return host_->widget()->IsVisible();
    }
    return false;
  }

  bool IsDrawn() override { return IsVisible(); }

 private:
  raw_ptr<CefOverlayViewHost> host_;
  CefRefPtr<CefView> view_;

  IMPLEMENT_REFCOUNTING(CefOverlayControllerImpl);
};

}  // namespace

CefOverlayViewHost::CefOverlayViewHost(CefWindowView* window_view,
                                       cef_docking_mode_t docking_mode)
    : window_view_(window_view), docking_mode_(docking_mode) {}

void CefOverlayViewHost::Init(views::View* host_view,
                              CefRefPtr<CefView> view,
                              bool can_activate) {
  DCHECK(view);

  // Match the logic in CEF_PANEL_IMPL_D::AddChildView().
  auto controls_view = view->IsAttached()
                           ? base::WrapUnique(view_util::GetFor(view))
                           : view_util::PassOwnership(view);
  DCHECK(controls_view.get());

  cef_controller_ = new CefOverlayControllerImpl(this, view);

  // Initialize the Widget. |widget_| will be deleted by the NativeWidget or
  // when WidgetDelegate::DeleteDelegate() deletes |this|.
  widget_ = std::make_unique<ThemeCopyingWidget>(window_view_->GetWidget());
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.delegate = this;
  params.name = "CefOverlayViewHost";
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = window_view_->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = can_activate
                           ? views::Widget::InitParams::Activatable::kYes
                           : views::Widget::InitParams::Activatable::kNo;
  widget_->Init(std::move(params));

  // |widget_| should now be associated with |this|.
  DCHECK_EQ(widget_.get(), GetWidget());

  // Make the Widget background transparent. The View might still be opaque.
  if (widget_->GetCompositor()) {
    widget_->GetCompositor()->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

  host_view_ = host_view;
  view_util::SetHostView(widget_.get(), host_view);

  // Cause WidgetDelegate::DeleteDelegate() to delete |this| after executing the
  // registered DeleteDelegate callback.
  SetOwnedByWidget(true);
  RegisterDeleteDelegateCallback(
      base::BindOnce(&CefOverlayViewHost::Cleanup, base::Unretained(this)));

  if (window_view_->IsChromeStyle()) {
    // Some attributes associated with a Chrome toolbar are located via the
    // Widget. See matching logic in BrowserView::AddedToWidget.
    auto browser_view = BrowserView::GetBrowserViewForNativeWindow(
        view_util::GetNativeWindow(window_view_->GetWidget()));
    if (browser_view) {
      widget_->SetNativeWindowProperty(BrowserView::kBrowserViewKey,
                                       browser_view);
    }
  }

  // Call AddChildView after the Widget properties have been configured.
  // Notifications resulting from this call may attempt to access those
  // properties (OnThemeChanged calling GetHostView, for example).
  view_ = widget_->GetContentsView()->AddChildView(std::move(controls_view));

  // Set the initial bounds after the View has been added to the Widget.
  // Otherwise, preferred size won't calculate correctly.
  gfx::Rect bounds;
  if (docking_mode_ == CEF_DOCKING_MODE_CUSTOM) {
    if (view_->size().IsEmpty()) {
      // Size to the preferred size to start.
      view_->SizeToPreferredSize();
    }

    // Top-left origin with existing size.
    bounds = gfx::Rect(gfx::Point(), view_->size());
  } else {
    bounds = ComputeBounds();
  }
  SetOverlayBounds(bounds);

  // Register for future bounds change notifications.
  view_->AddObserver(this);

  // Initially hidden.
  widget_->Hide();
}

void CefOverlayViewHost::Close() {
  if (widget_ && !widget_->IsClosed()) {
    // Remove all references ASAP, before the Widget is destroyed.
    Cleanup();

    // Eventually calls DeleteDelegate().
    widget_->Close();
  }
}

void CefOverlayViewHost::MoveIfNecessary() {
  if (bounds_changing_ || docking_mode_ == CEF_DOCKING_MODE_CUSTOM) {
    return;
  }
  SetOverlayBounds(ComputeBounds());
}

void CefOverlayViewHost::SetOverlayBounds(const gfx::Rect& bounds) {
  // Avoid re-entrancy of this method.
  if (bounds_changing_) {
    return;
  }

  // Empty bounds are not allowed.
  if (bounds.IsEmpty()) {
    return;
  }

  bounds_changing_ = true;
  bounds_ = bounds;

  // Keep the result inside the widget.
  bounds_.Intersect(window_view_->bounds());

  if (view_->size() != bounds_.size()) {
    view_->SetSize(bounds_.size());
  }
  widget_->SetBounds(bounds_);
  window_view_->OnOverlayBoundsChanged();

  bounds_changing_ = false;
}

void CefOverlayViewHost::SetOverlayInsets(const CefInsets& insets) {
  if (insets == insets_) {
    return;
  }
  insets_ = insets;
  MoveIfNecessary();
}

void CefOverlayViewHost::OnViewBoundsChanged(views::View* observed_view) {
  MoveIfNecessary();
}

void CefOverlayViewHost::OnViewIsDeleting(views::View* observed_view) {
  view_ = nullptr;
  Cleanup();
}

gfx::Rect CefOverlayViewHost::ComputeBounds() const {
  // This method is only used with corner docking.
  DCHECK_NE(docking_mode_, CEF_DOCKING_MODE_CUSTOM);

  // Find the area we have to work with.
  const auto& widget_bounds = window_view_->bounds();

  // Ask the view how large an area it needs to draw on.
  const auto& prefsize = view_->GetPreferredSize();

  // Swap left/right docking with RTL.
  const bool is_rtl = base::i18n::IsRTL();

  // Dock to the correct corner, considering insets in the docking corner only.
  int x = widget_bounds.x();
  int y = widget_bounds.y();
  if (((docking_mode_ == CEF_DOCKING_MODE_TOP_RIGHT ||
        docking_mode_ == CEF_DOCKING_MODE_BOTTOM_RIGHT) &&
       !is_rtl) ||
      ((docking_mode_ == CEF_DOCKING_MODE_TOP_LEFT ||
        docking_mode_ == CEF_DOCKING_MODE_BOTTOM_LEFT) &&
       is_rtl)) {
    x += widget_bounds.width() - prefsize.width() - insets_.right;
  } else {
    x += insets_.left;
  }
  if (docking_mode_ == CEF_DOCKING_MODE_BOTTOM_LEFT ||
      docking_mode_ == CEF_DOCKING_MODE_BOTTOM_RIGHT) {
    y += widget_bounds.height() - prefsize.height() - insets_.bottom;
  } else {
    y += insets_.top;
  }

  return gfx::Rect(x, y, prefsize.width(), prefsize.height());
}

void CefOverlayViewHost::Cleanup() {
  // This method may be called multiple times. For example, explicitly after the
  // client calls CefOverlayController::Destroy or implicitly when the host
  // Widget is being closed or destroyed. In most implicit cases
  // CefWindowView::WindowClosing will call this before the host Widget is
  // destroyed, allowing the client to optionally reuse the child View. However,
  // if CefWindowView::WindowClosing is not called, DeleteDelegate will call
  // this after the host Widget and all associated Widgets/Views have been
  // destroyed. In the DeleteDelegate case |widget_| will return nullptr.
  if (view_ && widget_) {
    // Remove the child View immediately. It may be reused by the client.
    auto view = view_util::GetFor(view_, /*find_known_parent=*/false);
    widget_->GetContentsView()->RemoveChildView(view_);
    if (view) {
      view_util::ResumeOwnership(view);
    }
    view_->RemoveObserver(this);
    view_ = nullptr;
  }

  if (cef_controller_) {
    CefOverlayControllerImpl* controller_impl =
        static_cast<CefOverlayControllerImpl*>(cef_controller_.get());
    controller_impl->Destroyed();
    cef_controller_ = nullptr;
  }

  if (window_view_) {
    window_view_->RemoveOverlayView(this, host_view_);
    window_view_ = nullptr;
    host_view_ = nullptr;
  }
}
