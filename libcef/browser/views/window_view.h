// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_WINDOW_VIEW_H_
#define CEF_LIBCEF_BROWSER_VIEWS_WINDOW_VIEW_H_
#pragma once

#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

#include "libcef/browser/views/panel_view.h"

#include "ui/display/display.h"
#include "ui/views/widget/widget_delegate.h"

class SkRegion;

// Manages the views-based root window. This object will be deleted
// automatically when the associated root window is destroyed.
class CefWindowView :
    public CefPanelView<views::WidgetDelegateView, CefWindowDelegate> {
 public:
  typedef CefPanelView<views::WidgetDelegateView, CefWindowDelegate>
      ParentClass;

  class Delegate {
   public:
    // Returns true to signal that the Widget can be closed.
    virtual bool CanWidgetClose() = 0;

    // Called when the underlying platform window is closing.
    virtual void OnWindowClosing() = 0;

    // Called when the WindowView is about to be deleted.
    virtual void OnWindowViewDeleted() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // |cef_delegate| may be nullptr.
  // |window_delegate| must be non-nullptr.
  CefWindowView(CefWindowDelegate* cef_delegate,
                Delegate* window_delegate);

  // Create the Widget.
  void CreateWidget();

  // Returns the CefWindow associated with this view. See comments on
  // CefViewView::GetCefView.
  CefRefPtr<CefWindow> GetCefWindow() const;

  // views::WidgetDelegateView methods:
  void DeleteDelegate() override;

  // views::WidgetDelegate methods:
  bool CanResize() const override;
  bool CanMinimize() const override;
  bool CanMaximize() const override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  gfx::ImageSkia GetWindowAppIcon() override;
  void WindowClosing() override;
  views::View* GetContentsView() override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  // views::View methods:
  void ViewHierarchyChanged(
      const views::View::ViewHierarchyChangedDetails& details) override;

  // Returns the Display containing this Window.
  display::Display GetDisplay() const;

  // Set/get the window title.
  void SetTitle(const base::string16& title);
  base::string16 title() const { return title_; }

  // Set/get the window icon. This should be a 16x16 icon suitable for use in
  // the Windows's title bar.
  void SetWindowIcon(CefRefPtr<CefImage> window_icon);
  CefRefPtr<CefImage> window_icon() const { return window_icon_; }

  // Set/get the window app icon. This should be a larger icon for use in the
  // host environment app switching UI. On Windows, this is the ICON_BIG used in
  // Alt-Tab list and Windows taskbar. The Window icon will be used by default
  // if no Window App icon is specified.
  void SetWindowAppIcon(CefRefPtr<CefImage> window_app_icon);
  CefRefPtr<CefImage> window_app_icon() const { return window_app_icon_; }

  // Set/get the draggable regions.
  void SetDraggableRegions(
      const std::vector<CefDraggableRegion>& regions);
  SkRegion* draggable_region() const { return draggable_region_.get(); }

  // Returns the NonClientFrameView for this Window. May be nullptr.
  views::NonClientFrameView* GetNonClientFrameView() const;

 private:
  // Not owned by this object.
  Delegate* window_delegate_;

  // True if the window is frameless. It might still be resizable and draggable.
  bool is_frameless_;

  base::string16 title_;
  CefRefPtr<CefImage> window_icon_;
  CefRefPtr<CefImage> window_app_icon_;

  std::unique_ptr<SkRegion> draggable_region_;

  DISALLOW_COPY_AND_ASSIGN(CefWindowView);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_WINDOW_VIEW_H_
