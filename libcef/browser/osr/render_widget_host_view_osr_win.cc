// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <windows.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"

#include "third_party/blink/public/platform/web_cursor_info.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

namespace {

class CefCompositorHostWin : public gfx::WindowImpl {
 public:
  CefCompositorHostWin() {
    // Create a hidden 1x1 borderless window.
    set_window_style(WS_POPUP | WS_SYSMENU);
    Init(NULL, gfx::Rect(0, 0, 1, 1));
  }

  ~CefCompositorHostWin() override { DestroyWindow(hwnd()); }

 private:
  CR_BEGIN_MSG_MAP_EX(CefCompositorHostWin)
    CR_MSG_WM_PAINT(OnPaint)
  CR_END_MSG_MAP()

  CR_MSG_MAP_CLASS_DECLARATIONS(CefCompositorHostWin)

  void OnPaint(HDC dc) { ValidateRect(hwnd(), NULL); }

  DISALLOW_COPY_AND_ASSIGN(CefCompositorHostWin);
};

// From content/common/cursors/webcursor_win.cc.

using blink::WebCursorInfo;

LPCWSTR ToCursorID(ui::CursorType type) {
  switch (type) {
    case ui::CursorType::kPointer:
      return IDC_ARROW;
    case ui::CursorType::kCross:
      return IDC_CROSS;
    case ui::CursorType::kHand:
      return IDC_HAND;
    case ui::CursorType::kIBeam:
      return IDC_IBEAM;
    case ui::CursorType::kWait:
      return IDC_WAIT;
    case ui::CursorType::kHelp:
      return IDC_HELP;
    case ui::CursorType::kEastResize:
      return IDC_SIZEWE;
    case ui::CursorType::kNorthResize:
      return IDC_SIZENS;
    case ui::CursorType::kNorthEastResize:
      return IDC_SIZENESW;
    case ui::CursorType::kNorthWestResize:
      return IDC_SIZENWSE;
    case ui::CursorType::kSouthResize:
      return IDC_SIZENS;
    case ui::CursorType::kSouthEastResize:
      return IDC_SIZENWSE;
    case ui::CursorType::kSouthWestResize:
      return IDC_SIZENESW;
    case ui::CursorType::kWestResize:
      return IDC_SIZEWE;
    case ui::CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case ui::CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case ui::CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case ui::CursorType::kNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case ui::CursorType::kColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case ui::CursorType::kRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case ui::CursorType::kMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case ui::CursorType::kEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case ui::CursorType::kNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case ui::CursorType::kNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case ui::CursorType::kNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case ui::CursorType::kSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case ui::CursorType::kSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case ui::CursorType::kSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case ui::CursorType::kWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case ui::CursorType::kMove:
      return IDC_SIZEALL;
    case ui::CursorType::kVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case ui::CursorType::kCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case ui::CursorType::kContextMenu:
      return IDC_ARROW;
    case ui::CursorType::kAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case ui::CursorType::kProgress:
      return IDC_APPSTARTING;
    case ui::CursorType::kNoDrop:
      return IDC_NO;
    case ui::CursorType::kCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case ui::CursorType::kNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case ui::CursorType::kNotAllowed:
      return IDC_NO;
    case ui::CursorType::kZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case ui::CursorType::kZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case ui::CursorType::kGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case ui::CursorType::kGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
    case ui::CursorType::kNull:
      return IDC_NO;
    case ui::CursorType::kMiddlePanningVertical:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_VERTICAL);
    case ui::CursorType::kMiddlePanningHorizontal:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_HORIZONTAL);
    // TODO(cef): Find better cursors for these things
    case ui::CursorType::kDndNone:
      return IDC_ARROW;
    case ui::CursorType::kDndMove:
      return IDC_ARROW;
    case ui::CursorType::kDndCopy:
      return IDC_ARROW;
    case ui::CursorType::kDndLink:
      return IDC_ARROW;
    case ui::CursorType::kCustom:
      break;
  }
  NOTREACHED();
  return NULL;
}

bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

}  // namespace

ui::PlatformCursor CefRenderWidgetHostViewOSR::GetPlatformCursor(
    ui::CursorType type) {
  HMODULE module_handle = NULL;
  const wchar_t* cursor_id = ToCursorID(type);
  if (!IsSystemCursorID(cursor_id)) {
    module_handle =
        ::GetModuleHandle(CefContentBrowserClient::Get()->GetResourceDllName());
    if (!module_handle)
      module_handle = ::GetModuleHandle(NULL);
  }

  return LoadCursor(module_handle, cursor_id);
}
