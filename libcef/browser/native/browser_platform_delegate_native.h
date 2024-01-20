// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"

// Base implementation of native browser functionality.
class CefBrowserPlatformDelegateNative
    : public CefBrowserPlatformDelegateAlloy {
 public:
  // Used by the windowless implementation to override specific functionality
  // when delegating to the native implementation.
  class WindowlessHandler {
   public:
    // Returns the parent window handle.
    virtual CefWindowHandle GetParentWindowHandle() const = 0;

    // Convert from view DIP coordinates to screen coordinates. If
    // |want_dip_coords| is true return DIP instead of device (pixel)
    // coordinates on Windows/Linux.
    virtual gfx::Point GetParentScreenPoint(const gfx::Point& view,
                                            bool want_dip_coords) const = 0;

   protected:
    virtual ~WindowlessHandler() = default;
  };

  // CefBrowserPlatformDelegate methods:
  SkColor GetBackgroundColor() const override;
  void WasResized() override;

  // Translate CEF events to Chromium/Blink Web events.
  virtual content::NativeWebKeyboardEvent TranslateWebKeyEvent(
      const CefKeyEvent& key_event) const = 0;
  virtual blink::WebMouseEvent TranslateWebClickEvent(
      const CefMouseEvent& mouse_event,
      CefBrowserHost::MouseButtonType type,
      bool mouseUp,
      int clickCount) const = 0;
  virtual blink::WebMouseEvent TranslateWebMoveEvent(
      const CefMouseEvent& mouse_event,
      bool mouseLeave) const = 0;
  virtual blink::WebMouseWheelEvent TranslateWebWheelEvent(
      const CefMouseEvent& mouse_event,
      int deltaX,
      int deltaY) const = 0;

  const CefWindowInfo& window_info() const { return window_info_; }

 protected:
  // Delegates that can wrap a native delegate.
  friend class CefBrowserPlatformDelegateBackground;
  friend class CefBrowserPlatformDelegateChrome;
  friend class CefBrowserPlatformDelegateOsr;
  friend class CefBrowserPlatformDelegateViews;

  CefBrowserPlatformDelegateNative(const CefWindowInfo& window_info,
                                   SkColor background_color);

  // Methods used by delegates that can wrap a native delegate.
  void set_windowless_handler(WindowlessHandler* handler) {
    windowless_handler_ = handler;
    set_as_secondary();
  }

  CefWindowInfo window_info_;
  const SkColor background_color_;

  // Not owned by this object.
  WindowlessHandler* windowless_handler_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
