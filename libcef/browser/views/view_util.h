// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_VIEW_UTIL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_VIEW_UTIL_H_
#pragma once

#include "include/views/cef_view.h"
#include "include/views/cef_window.h"

#include "ui/gfx/native_widget_types.h"
#include "ui/views/view.h"

namespace display {
class Display;
}

namespace gfx {
class Point;
}

namespace views {
class NativeWidget;
class View;
class Widget;
namespace internal {
class NativeWidgetDelegate;
}
}  // namespace views

class CefWindowDelegate;

#define CEF_REQUIRE_VALID_RETURN(ret) \
  if (!ParentClass::IsValid())        \
    return ret;

#define CEF_REQUIRE_VALID_RETURN_VOID() \
  if (!ParentClass::IsValid())          \
    return;

// The below functions manage the relationship between CefView and views::View
// instances. See comments in view_impl.h for a usage overview.

namespace view_util {

// Default values.
extern const SkColor kDefaultBackgroundColor;
extern const char kDefaultFontList[];

// Called when a CefView is initialized to create the initial association
// between the underlying views::View and |view|. The CefView owns the
// views::View at this stage.
void Register(CefRefPtr<CefView> view);

// Returns the CefView object associated with the specified |view|. If no
// CefView is associated with |view| and |find_known_parent| is true then this
// function will return the closest parent views::View with an associated
// CefView.
CefRefPtr<CefView> GetFor(const views::View* view, bool find_known_parent);

// Returns the views::View object associated with the specified |view|.
// Ownership of the views::View object does not change.
views::View* GetFor(CefRefPtr<CefView> view);

// Returns the views::View object associated with the specified |view| and
// passes ownership to the caller. The views::View object should then be passed
// to another views::View via views::View or views::LayoutManager methods. The
// views::View will keep a ref-counted reference to |view|, and |view| will keep
// an un-owned reference to the views::View. These references will reset when
// the views::View object is deleted or when ResumeOwnership() is called.
[[nodiscard]] std::unique_ptr<views::View> PassOwnership(
    CefRefPtr<CefView> view);

// Causes |view| to resume ownership of the views::View object. Should be called
// after removing the views::View object from its previous parent.
void ResumeOwnership(CefRefPtr<CefView> view);

// Returns the Window associated with |widget|.
CefRefPtr<CefWindow> GetWindowFor(views::Widget* widget);

// Returns the Display nearest |point|. Set |input_pixel_coords| to true if
// |point| is in pixel coordinates instead of density independent pixels (DIP).
display::Display GetDisplayNearestPoint(const gfx::Point& point,
                                        bool input_pixel_coords);

// Returns the Display that most closely intersects |bounds|.  Set
// |input_pixel_coords| to true if |bounds| is in pixel coordinates instead of
// density independent pixels (DIP).
display::Display GetDisplayMatchingBounds(const gfx::Rect& bounds,
                                          bool input_pixel_coords);

// Convert |point| from pixel coordinates to density independent pixels (DIP)
// using |device_scale_factor|.
void ConvertPointFromPixels(gfx::Point* point, float device_scale_factor);

// Convert |point| to pixel coordinates from density independent pixels (DIP)
// using |device_scale_factor|.
void ConvertPointToPixels(gfx::Point* point, float device_scale_factor);

#if BUILDFLAG(IS_WIN)
// Convert |point| from pixel screen coordinates to DIP screen coordinates.
gfx::Point ConvertPointFromPixels(const gfx::Point& point);

// Convert |point| from DIP screen coordinates to pixel screen coordinates.
gfx::Point ConvertPointToPixels(const gfx::Point& point);

// Convert |rect| from pixel screen coordinates to DIP screen coordinates.
gfx::Rect ConvertRectFromPixels(const gfx::Rect& rect);

// Convert |rect| from DIP screen coordinates to pixel screen coordinates.
gfx::Rect ConvertRectToPixels(const gfx::Rect& rect);
#endif  // BUILDFLAG(IS_WIN)

// Convert |point| from |view| to screen coordinates. If |output_pixel_coords|
// is true then |point| will be output in pixel coordinates instead of density
// independent pixels (DIP). Returns false if |view| does not currently belong
// to a Widget.
bool ConvertPointToScreen(views::View* view,
                          gfx::Point* point,
                          bool output_pixel_coords);

// Convert |point| from screen to |view| coordinates. Set |input_pixel_coords|
// to true when |point| is being input in pixel coordinates instead of density
// independent pixels (DIP). Returns false if |view| does not currently belong
// to a Widget.
bool ConvertPointFromScreen(views::View* view,
                            gfx::Point* point,
                            bool input_pixel_coords);

// Convert |point| from |view| to window (Widget) coordinates. Returns false if
// |view| does not currently belong to a Widget.
bool ConvertPointToWindow(views::View* view, gfx::Point* point);

// Convert |point| from window (Widget) to |view| coordinates. Returns false if
// |view| does not currently belong to a Widget.
bool ConvertPointFromWindow(views::View* view, gfx::Point* point);

// Returns the native window handle for |widget|. May return nullptr.
gfx::NativeWindow GetNativeWindow(views::Widget* widget);

// Returns the native view handle for |widget|. May return nullptr.
gfx::NativeView GetNativeView(views::Widget* widget);

// Returns the platform window handle for |widget|. May return nullptr.
CefWindowHandle GetWindowHandle(views::Widget* widget);

// Returns the platform window handle for |window|. May return nullptr.
CefWindowHandle GetWindowHandle(gfx::NativeWindow window);

views::NativeWidget* CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate,
    CefRefPtr<CefWindow> window,
    CefWindowDelegate* window_delegate);

// Called from CefOverlayViewHost::Init to associate |host_view| with |widget|.
// This is necessary for GetWindowFor() to correctly return the CefWindow
// associated with the host Widget. On Aura platforms, |host_view| is the view
// whose position in the view hierarchy determines the z-order of the widget
// relative to views with layers and views with associated NativeViews.
void SetHostView(views::Widget* widget, views::View* host_view);
views::View* GetHostView(views::Widget* widget);

#if BUILDFLAG(IS_MAC)
float GetNSWindowTitleBarHeight(views::Widget* widget);
#endif

}  // namespace view_util

#endif  // CEF_LIBCEF_BROWSER_VIEWS_VIEW_UTIL_H_
