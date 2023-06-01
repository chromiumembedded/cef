// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_aura.h"

#include "libcef/browser/native/menu_runner_views_aura.h"
#include "libcef/browser/views/view_util.h"

#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"

CefBrowserPlatformDelegateNativeAura::CefBrowserPlatformDelegateNativeAura(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNative(window_info, background_color) {}

void CefBrowserPlatformDelegateNativeAura::SendKeyEvent(
    const CefKeyEvent& event) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  ui::KeyEvent ui_event = TranslateUiKeyEvent(event);
  view->OnKeyEvent(&ui_event);
}

void CefBrowserPlatformDelegateNativeAura::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  ui::MouseEvent ui_event =
      TranslateUiClickEvent(event, type, mouseUp, clickCount);
  view->OnMouseEvent(&ui_event);
}

void CefBrowserPlatformDelegateNativeAura::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  ui::MouseEvent ui_event = TranslateUiMoveEvent(event, mouseLeave);
  view->OnMouseEvent(&ui_event);
}

void CefBrowserPlatformDelegateNativeAura::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  ui::MouseWheelEvent ui_event = TranslateUiWheelEvent(event, deltaX, deltaY);
  view->OnMouseEvent(&ui_event);
}

void CefBrowserPlatformDelegateNativeAura::SendTouchEvent(
    const CefTouchEvent& event) {
  NOTIMPLEMENTED();
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateNativeAura::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerViewsAura);
}

gfx::Point CefBrowserPlatformDelegateNativeAura::GetScreenPoint(
    const gfx::Point& view,
    bool want_dip_coords) const {
  if (windowless_handler_) {
    return windowless_handler_->GetParentScreenPoint(view, want_dip_coords);
  }

  if (!window_widget_) {
    return view;
  }

  gfx::Point screen_pt(view);
  if (!view_util::ConvertPointToScreen(
          window_widget_->GetRootView(), &screen_pt,
          /*output_pixel_coords=*/!want_dip_coords)) {
    return view;
  }

  return screen_pt;
}

content::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNativeAura::TranslateWebKeyEvent(
    const CefKeyEvent& key_event) const {
  return content::NativeWebKeyboardEvent(TranslateUiKeyEvent(key_event));
}

blink::WebMouseEvent
CefBrowserPlatformDelegateNativeAura::TranslateWebClickEvent(
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) const {
  return ui::MakeWebMouseEvent(
      TranslateUiClickEvent(mouse_event, type, mouseUp, clickCount));
}

blink::WebMouseEvent
CefBrowserPlatformDelegateNativeAura::TranslateWebMoveEvent(
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  return ui::MakeWebMouseEvent(TranslateUiMoveEvent(mouse_event, mouseLeave));
}

blink::WebMouseWheelEvent
CefBrowserPlatformDelegateNativeAura::TranslateWebWheelEvent(
    const CefMouseEvent& mouse_event,
    int deltaX,
    int deltaY) const {
  return ui::MakeWebMouseWheelEvent(
      TranslateUiWheelEvent(mouse_event, deltaX, deltaY));
}

ui::MouseEvent CefBrowserPlatformDelegateNativeAura::TranslateUiClickEvent(
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) const {
  DCHECK_GE(clickCount, 1);

  ui::EventType event_type =
      mouseUp ? ui::ET_MOUSE_RELEASED : ui::ET_MOUSE_PRESSED;
  gfx::PointF location(mouse_event.x, mouse_event.y);
  gfx::PointF root_location(GetScreenPoint(
      gfx::Point(mouse_event.x, mouse_event.y), /*want_dip_coords=*/false));
  base::TimeTicks time_stamp = GetEventTimeStamp();
  int flags = TranslateUiEventModifiers(mouse_event.modifiers);

  int changed_button_flags = 0;
  switch (type) {
    case MBT_LEFT:
      changed_button_flags |= ui::EF_LEFT_MOUSE_BUTTON;
      break;
    case MBT_MIDDLE:
      changed_button_flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
      break;
    case MBT_RIGHT:
      changed_button_flags |= ui::EF_RIGHT_MOUSE_BUTTON;
      break;
    default:
      DCHECK(false);
  }

  ui::MouseEvent result(event_type, location, root_location, time_stamp, flags,
                        changed_button_flags);
  result.SetClickCount(clickCount);
  return result;
}

ui::MouseEvent CefBrowserPlatformDelegateNativeAura::TranslateUiMoveEvent(
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  ui::EventType event_type =
      mouseLeave ? ui::ET_MOUSE_EXITED : ui::ET_MOUSE_MOVED;
  gfx::PointF location(mouse_event.x, mouse_event.y);
  gfx::PointF root_location(GetScreenPoint(
      gfx::Point(mouse_event.x, mouse_event.y), /*want_dip_coords=*/false));
  base::TimeTicks time_stamp = GetEventTimeStamp();
  int flags = TranslateUiEventModifiers(mouse_event.modifiers);

  int changed_button_flags = 0;
  if (!mouseLeave) {
    changed_button_flags = TranslateUiChangedButtonFlags(mouse_event.modifiers);
  }

  return ui::MouseEvent(event_type, location, root_location, time_stamp, flags,
                        changed_button_flags);
}

ui::MouseWheelEvent CefBrowserPlatformDelegateNativeAura::TranslateUiWheelEvent(
    const CefMouseEvent& mouse_event,
    int deltaX,
    int deltaY) const {
  gfx::Vector2d offset(GetUiWheelEventOffset(deltaX, deltaY));
  DCHECK(!offset.IsZero());

  gfx::PointF location(mouse_event.x, mouse_event.y);
  gfx::PointF root_location(GetScreenPoint(
      gfx::Point(mouse_event.x, mouse_event.y), /*want_dip_coords=*/false));
  base::TimeTicks time_stamp = GetEventTimeStamp();
  int flags = TranslateUiEventModifiers(mouse_event.modifiers);
  int changed_button_flags =
      TranslateUiChangedButtonFlags(mouse_event.modifiers);

  return ui::MouseWheelEvent(offset, location, root_location, time_stamp,
                             (ui::EF_PRECISION_SCROLLING_DELTA | flags),
                             changed_button_flags);
}

gfx::Vector2d CefBrowserPlatformDelegateNativeAura::GetUiWheelEventOffset(
    int deltaX,
    int deltaY) const {
  return gfx::Vector2d(deltaX, deltaY);
}

base::OnceClosure
CefBrowserPlatformDelegateNativeAura::GetWidgetDeleteCallback() {
  return base::BindOnce(&CefBrowserPlatformDelegateNativeAura::WidgetDeleted,
                        weak_ptr_factory_.GetWeakPtr());
}

// static
base::TimeTicks CefBrowserPlatformDelegateNativeAura::GetEventTimeStamp() {
  return base::TimeTicks::Now();
}

// static
int CefBrowserPlatformDelegateNativeAura::TranslateUiEventModifiers(
    uint32_t cef_modifiers) {
  int result = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON) {
    result |= ui::EF_CAPS_LOCK_ON;
  }
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN) {
    result |= ui::EF_SHIFT_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN) {
    result |= ui::EF_CONTROL_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_ALT_DOWN) {
    result |= ui::EF_ALT_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
    result |= ui::EF_LEFT_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
    result |= ui::EF_MIDDLE_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
    result |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN) {
    result |= ui::EF_COMMAND_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON) {
    result |= ui::EF_NUM_LOCK_ON;
  }
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD) {
    result |= ui::EF_IS_EXTENDED_KEY;
  }
  if (cef_modifiers & EVENTFLAG_ALTGR_DOWN) {
    result |= ui::EF_ALTGR_DOWN;
  }
  if (cef_modifiers & EVENTFLAG_IS_REPEAT) {
    result |= ui::EF_IS_REPEAT;
  }
  return result;
}

// static
int CefBrowserPlatformDelegateNativeAura::TranslateUiChangedButtonFlags(
    uint32_t cef_modifiers) {
  int result = 0;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
    result |= ui::EF_LEFT_MOUSE_BUTTON;
  } else if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
    result |= ui::EF_MIDDLE_MOUSE_BUTTON;
  } else if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
    result |= ui::EF_RIGHT_MOUSE_BUTTON;
  }
  return result;
}

void CefBrowserPlatformDelegateNativeAura::WidgetDeleted() {
  DCHECK(window_widget_);
  window_widget_ = nullptr;
}

content::RenderWidgetHostViewAura*
CefBrowserPlatformDelegateNativeAura::GetHostView() const {
  if (!web_contents_) {
    return nullptr;
  }
  return static_cast<content::RenderWidgetHostViewAura*>(
      web_contents_->GetRenderWidgetHostView());
}
