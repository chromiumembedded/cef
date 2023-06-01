// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA_H_
#define CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA_H_

#include "libcef/browser/native/browser_platform_delegate_native.h"

#include "base/memory/weak_ptr.h"
#include "ui/events/event.h"

namespace content {
class RenderWidgetHostViewAura;
}

namespace gfx {
class Vector2d;
}

// Windowed browser implementation for Aura platforms.
class CefBrowserPlatformDelegateNativeAura
    : public CefBrowserPlatformDelegateNative {
 public:
  CefBrowserPlatformDelegateNativeAura(const CefWindowInfo& window_info,
                                       SkColor background_color);

  // CefBrowserPlatformDelegate methods:
  void SendKeyEvent(const CefKeyEvent& event) override;
  void SendMouseClickEvent(const CefMouseEvent& event,
                           CefBrowserHost::MouseButtonType type,
                           bool mouseUp,
                           int clickCount) override;
  void SendMouseMoveEvent(const CefMouseEvent& event, bool mouseLeave) override;
  void SendMouseWheelEvent(const CefMouseEvent& event,
                           int deltaX,
                           int deltaY) override;
  void SendTouchEvent(const CefTouchEvent& event) override;
  std::unique_ptr<CefMenuRunner> CreateMenuRunner() override;
  gfx::Point GetScreenPoint(const gfx::Point& view,
                            bool want_dip_coords) const override;

  // CefBrowserPlatformDelegateNative methods:
  content::NativeWebKeyboardEvent TranslateWebKeyEvent(
      const CefKeyEvent& key_event) const override;
  blink::WebMouseEvent TranslateWebClickEvent(
      const CefMouseEvent& mouse_event,
      CefBrowserHost::MouseButtonType type,
      bool mouseUp,
      int clickCount) const override;
  blink::WebMouseEvent TranslateWebMoveEvent(const CefMouseEvent& mouse_event,
                                             bool mouseLeave) const override;
  blink::WebMouseWheelEvent TranslateWebWheelEvent(
      const CefMouseEvent& mouse_event,
      int deltaX,
      int deltaY) const override;

  // Translate CEF events to Chromium UI events.
  virtual ui::KeyEvent TranslateUiKeyEvent(
      const CefKeyEvent& key_event) const = 0;
  virtual ui::MouseEvent TranslateUiClickEvent(
      const CefMouseEvent& mouse_event,
      CefBrowserHost::MouseButtonType type,
      bool mouseUp,
      int clickCount) const;
  virtual ui::MouseEvent TranslateUiMoveEvent(const CefMouseEvent& mouse_event,
                                              bool mouseLeave) const;
  virtual ui::MouseWheelEvent TranslateUiWheelEvent(
      const CefMouseEvent& mouse_event,
      int deltaX,
      int deltaY) const;
  virtual gfx::Vector2d GetUiWheelEventOffset(int deltaX, int deltaY) const;

 protected:
  base::OnceClosure GetWidgetDeleteCallback();

  static base::TimeTicks GetEventTimeStamp();
  static int TranslateUiEventModifiers(uint32_t cef_modifiers);
  static int TranslateUiChangedButtonFlags(uint32_t cef_modifiers);

  // Widget hosting the web contents. It will be deleted automatically when the
  // associated root window is destroyed.
  views::Widget* window_widget_ = nullptr;

 private:
  // Will only be called if the Widget is deleted before
  // CefBrowserHostBase::DestroyBrowser() is called.
  void WidgetDeleted();

  content::RenderWidgetHostViewAura* GetHostView() const;

  base::WeakPtrFactory<CefBrowserPlatformDelegateNativeAura> weak_ptr_factory_{
      this};
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_NATIVE_AURA_H_
