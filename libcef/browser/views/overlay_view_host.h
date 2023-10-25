// Copyright 2021 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_OVERLAY_VIEW_HOST_H_
#define CEF_LIBCEF_BROWSER_VIEWS_OVERLAY_VIEW_HOST_H_
#pragma once

#include <memory>

#include "include/views/cef_overlay_controller.h"
#include "include/views/cef_view.h"

#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_delegate.h"

class CefWindowView;

// Host class for a child Widget that behaves as an overlay control. Based on
// Chrome's DropdownBarHost.
class CefOverlayViewHost : public views::WidgetDelegate,
                           public views::ViewObserver {
 public:
  // |window_view| is the top-level view that contains this overlay.
  CefOverlayViewHost(CefWindowView* window_view,
                     cef_docking_mode_t docking_mode);

  CefOverlayViewHost(const CefOverlayViewHost&) = delete;
  CefOverlayViewHost& operator=(const CefOverlayViewHost&) = delete;

  // Initializes the CefOverlayViewHost. This creates the Widget that |view|
  // paints into. On Aura platforms, |host_view| is the view whose position in
  // the |window_view_| view hierarchy determines the z-order of the widget
  // relative to views with layers and views with associated NativeViews.
  void Init(views::View* host_view, CefRefPtr<CefView> view, bool can_activate);

  void Destroy();

  void MoveIfNecessary();

  void SetOverlayBounds(const gfx::Rect& bounds);
  void SetOverlayInsets(const CefInsets& insets);

  // views::ViewObserver methods:
  void OnViewBoundsChanged(views::View* observed_view) override;

  cef_docking_mode_t docking_mode() const { return docking_mode_; }
  CefRefPtr<CefOverlayController> controller() const { return cef_controller_; }
  CefWindowView* window_view() const { return window_view_; }
  views::Widget* widget() const { return widget_.get(); }
  views::View* view() const { return view_; }
  gfx::Rect bounds() const { return bounds_; }
  CefInsets insets() const { return insets_; }

 private:
  gfx::Rect ComputeBounds() const;

  // The CefWindowView that created us.
  CefWindowView* const window_view_;

  const cef_docking_mode_t docking_mode_;

  // Our view, which is responsible for drawing the UI.
  views::View* view_ = nullptr;

  // The Widget implementation that is created and maintained by the overlay.
  // It contains |view_|.
  std::unique_ptr<views::Widget> widget_;

  CefRefPtr<CefOverlayController> cef_controller_;

  gfx::Rect bounds_;
  bool bounds_changing_ = false;

  CefInsets insets_;
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_OVERLAY_VIEW_HOST_H_
