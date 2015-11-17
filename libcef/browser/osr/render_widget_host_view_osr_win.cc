// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <windows.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"

#include "grit/ui_unscaled_resources.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"

namespace {

class CefCompositorHostWin : public gfx::WindowImpl {
 public:
  CefCompositorHostWin() {
    // Create a hidden 1x1 borderless window.
    set_window_style(WS_POPUP | WS_SYSMENU);
    Init(NULL, gfx::Rect(0, 0, 1, 1));
  }

  ~CefCompositorHostWin() override {
    DestroyWindow(hwnd());
  }

 private:
  CR_BEGIN_MSG_MAP_EX(CompositorHostWin)
    CR_MSG_WM_PAINT(OnPaint)
  CR_END_MSG_MAP()

  void OnPaint(HDC dc) {
    ValidateRect(hwnd(), NULL);
  }

  DISALLOW_COPY_AND_ASSIGN(CefCompositorHostWin);
};


// From content/common/cursors/webcursor_win.cc.

using blink::WebCursorInfo;

LPCWSTR ToCursorID(WebCursorInfo::Type type) {
  switch (type) {
    case WebCursorInfo::TypePointer:
      return IDC_ARROW;
    case WebCursorInfo::TypeCross:
      return IDC_CROSS;
    case WebCursorInfo::TypeHand:
      return IDC_HAND;
    case WebCursorInfo::TypeIBeam:
      return IDC_IBEAM;
    case WebCursorInfo::TypeWait:
      return IDC_WAIT;
    case WebCursorInfo::TypeHelp:
      return IDC_HELP;
    case WebCursorInfo::TypeEastResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeNorthEastResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::TypeEastWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::TypeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case WebCursorInfo::TypeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case WebCursorInfo::TypeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case WebCursorInfo::TypeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case WebCursorInfo::TypeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case WebCursorInfo::TypeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case WebCursorInfo::TypeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case WebCursorInfo::TypeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case WebCursorInfo::TypeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case WebCursorInfo::TypeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case WebCursorInfo::TypeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case WebCursorInfo::TypeMove:
      return IDC_SIZEALL;
    case WebCursorInfo::TypeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case WebCursorInfo::TypeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case WebCursorInfo::TypeContextMenu:
      return IDC_ARROW;
    case WebCursorInfo::TypeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case WebCursorInfo::TypeProgress:
      return IDC_APPSTARTING;
    case WebCursorInfo::TypeNoDrop:
      return IDC_NO;
    case WebCursorInfo::TypeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case WebCursorInfo::TypeNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case WebCursorInfo::TypeNotAllowed:
      return IDC_NO;
    case WebCursorInfo::TypeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case WebCursorInfo::TypeZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case WebCursorInfo::TypeGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case WebCursorInfo::TypeGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
  }
  NOTREACHED();
  return NULL;
}

bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

}  // namespace

void CefRenderWidgetHostViewOSR::SetParentNativeViewAccessible(
    gfx::NativeViewAccessible accessible_parent) {
}

gfx::NativeViewId
    CefRenderWidgetHostViewOSR::GetParentForWindowlessPlugin() const {
  if (browser_impl_.get()) {
    return reinterpret_cast<gfx::NativeViewId>(
        browser_impl_->GetWindowHandle());
  }
  return NULL;
}

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget() {
  DCHECK(!window_);
  window_.reset(new CefCompositorHostWin());
  compositor_widget_ = window_->hwnd();
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  window_.reset(NULL);
  compositor_widget_ = gfx::kNullAcceleratedWidget;
}

ui::PlatformCursor CefRenderWidgetHostViewOSR::GetPlatformCursor(
    blink::WebCursorInfo::Type type) {
  HMODULE module_handle = NULL;
  const wchar_t* cursor_id = ToCursorID(type);
  if (!IsSystemCursorID(cursor_id)) {
    module_handle = ::GetModuleHandle(
        CefContentBrowserClient::Get()->GetResourceDllName());
    if (!module_handle)
      module_handle = ::GetModuleHandle(NULL);
  }

  return LoadCursor(module_handle, cursor_id);
}
