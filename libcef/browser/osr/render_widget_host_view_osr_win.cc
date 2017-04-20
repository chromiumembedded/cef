// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <windows.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"

#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

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
    case WebCursorInfo::kTypePointer:
      return IDC_ARROW;
    case WebCursorInfo::kTypeCross:
      return IDC_CROSS;
    case WebCursorInfo::kTypeHand:
      return IDC_HAND;
    case WebCursorInfo::kTypeIBeam:
      return IDC_IBEAM;
    case WebCursorInfo::kTypeWait:
      return IDC_WAIT;
    case WebCursorInfo::kTypeHelp:
      return IDC_HELP;
    case WebCursorInfo::kTypeEastResize:
      return IDC_SIZEWE;
    case WebCursorInfo::kTypeNorthResize:
      return IDC_SIZENS;
    case WebCursorInfo::kTypeNorthEastResize:
      return IDC_SIZENESW;
    case WebCursorInfo::kTypeNorthWestResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::kTypeSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::kTypeSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::kTypeSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::kTypeWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::kTypeNorthSouthResize:
      return IDC_SIZENS;
    case WebCursorInfo::kTypeEastWestResize:
      return IDC_SIZEWE;
    case WebCursorInfo::kTypeNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case WebCursorInfo::kTypeNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case WebCursorInfo::kTypeColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case WebCursorInfo::kTypeRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case WebCursorInfo::kTypeMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case WebCursorInfo::kTypeEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case WebCursorInfo::kTypeNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case WebCursorInfo::kTypeNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case WebCursorInfo::kTypeNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case WebCursorInfo::kTypeSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case WebCursorInfo::kTypeSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case WebCursorInfo::kTypeSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case WebCursorInfo::kTypeWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case WebCursorInfo::kTypeMove:
      return IDC_SIZEALL;
    case WebCursorInfo::kTypeVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case WebCursorInfo::kTypeCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case WebCursorInfo::kTypeContextMenu:
      return IDC_ARROW;
    case WebCursorInfo::kTypeAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case WebCursorInfo::kTypeProgress:
      return IDC_APPSTARTING;
    case WebCursorInfo::kTypeNoDrop:
      return IDC_NO;
    case WebCursorInfo::kTypeCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case WebCursorInfo::kTypeNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case WebCursorInfo::kTypeNotAllowed:
      return IDC_NO;
    case WebCursorInfo::kTypeZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case WebCursorInfo::kTypeZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case WebCursorInfo::kTypeGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case WebCursorInfo::kTypeGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
  }
  NOTREACHED();
  return NULL;
}

bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

}  // namespace

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget(
    bool is_guest_view_hack) {
  DCHECK(!window_);
  window_.reset(new CefCompositorHostWin());
  compositor_widget_ = window_->hwnd();
}

void CefRenderWidgetHostViewOSR::PlatformResizeCompositorWidget(
    const gfx::Size& size) {
  DCHECK(window_);
  SetWindowPos(window_->hwnd(), NULL, 0, 0, size.width(), size.height(),
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
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
