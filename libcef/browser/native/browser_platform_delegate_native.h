// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_

#include "libcef/browser/browser_platform_delegate.h"

// Base implementation of native browser functionality.
class CefBrowserPlatformDelegateNative : public CefBrowserPlatformDelegate {
 public:
  // Used by the windowless implementation to override specific functionality
  // when delegating to the native implementation.
  class WindowlessHandler {
   public:
    // Returns the parent window handle.
    virtual CefWindowHandle GetParentWindowHandle() const = 0;

    // Convert from view coordinates to screen coordinates.
    virtual gfx::Point GetParentScreenPoint(const gfx::Point& view) const = 0;

   protected:
    virtual ~WindowlessHandler() {}
  };

  // CefBrowserPlatformDelegate methods:
  bool CanUseSharedTexture() const override;
  bool CanUseExternalBeginFrame() const override;
  SkColor GetBackgroundColor() const override;
  void WasResized() override;
  bool IsWindowless() const override;
  bool IsViewsHosted() const override;

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
  friend class CefBrowserPlatformDelegateOsr;
  friend class CefBrowserPlatformDelegateViews;

  CefBrowserPlatformDelegateNative(const CefWindowInfo& window_info,
                                   SkColor background_color,
                                   bool use_shared_texture,
                                   bool use_external_begin_frame);

  // Methods used by delegates that can wrap a native delegate.
  void set_windowless_handler(WindowlessHandler* handler) {
    windowless_handler_ = handler;
  }
  void set_browser(CefBrowserHostImpl* browser) { browser_ = browser; }

  CefWindowInfo window_info_;
  const SkColor background_color_;
  const bool use_shared_texture_;
  const bool use_external_begin_frame_;

  WindowlessHandler* windowless_handler_;  // Not owned by this object.
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_H_
