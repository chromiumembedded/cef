// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <windows.h>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"

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

LPCWSTR ToCursorID(ui::mojom::CursorType type) {
  switch (type) {
    case ui::mojom::CursorType::kPointer:
      return IDC_ARROW;
    case ui::mojom::CursorType::kCross:
      return IDC_CROSS;
    case ui::mojom::CursorType::kHand:
      return IDC_HAND;
    case ui::mojom::CursorType::kIBeam:
      return IDC_IBEAM;
    case ui::mojom::CursorType::kWait:
      return IDC_WAIT;
    case ui::mojom::CursorType::kHelp:
      return IDC_HELP;
    case ui::mojom::CursorType::kEastResize:
      return IDC_SIZEWE;
    case ui::mojom::CursorType::kNorthResize:
      return IDC_SIZENS;
    case ui::mojom::CursorType::kNorthEastResize:
      return IDC_SIZENESW;
    case ui::mojom::CursorType::kNorthWestResize:
      return IDC_SIZENWSE;
    case ui::mojom::CursorType::kSouthResize:
      return IDC_SIZENS;
    case ui::mojom::CursorType::kSouthEastResize:
      return IDC_SIZENWSE;
    case ui::mojom::CursorType::kSouthWestResize:
      return IDC_SIZENESW;
    case ui::mojom::CursorType::kWestResize:
      return IDC_SIZEWE;
    case ui::mojom::CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case ui::mojom::CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case ui::mojom::CursorType::kNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case ui::mojom::CursorType::kColumnResize:
      return MAKEINTRESOURCE(IDC_COLRESIZE);
    case ui::mojom::CursorType::kRowResize:
      return MAKEINTRESOURCE(IDC_ROWRESIZE);
    case ui::mojom::CursorType::kMiddlePanning:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE);
    case ui::mojom::CursorType::kEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_EAST);
    case ui::mojom::CursorType::kNorthPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH);
    case ui::mojom::CursorType::kNorthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_EAST);
    case ui::mojom::CursorType::kNorthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_NORTH_WEST);
    case ui::mojom::CursorType::kSouthPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH);
    case ui::mojom::CursorType::kSouthEastPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_EAST);
    case ui::mojom::CursorType::kSouthWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_SOUTH_WEST);
    case ui::mojom::CursorType::kWestPanning:
      return MAKEINTRESOURCE(IDC_PAN_WEST);
    case ui::mojom::CursorType::kMove:
      return IDC_SIZEALL;
    case ui::mojom::CursorType::kVerticalText:
      return MAKEINTRESOURCE(IDC_VERTICALTEXT);
    case ui::mojom::CursorType::kCell:
      return MAKEINTRESOURCE(IDC_CELL);
    case ui::mojom::CursorType::kContextMenu:
      return IDC_ARROW;
    case ui::mojom::CursorType::kAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case ui::mojom::CursorType::kProgress:
      return IDC_APPSTARTING;
    case ui::mojom::CursorType::kNoDrop:
      return IDC_NO;
    case ui::mojom::CursorType::kCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
    case ui::mojom::CursorType::kNone:
      return MAKEINTRESOURCE(IDC_CURSOR_NONE);
    case ui::mojom::CursorType::kNotAllowed:
      return IDC_NO;
    case ui::mojom::CursorType::kZoomIn:
      return MAKEINTRESOURCE(IDC_ZOOMIN);
    case ui::mojom::CursorType::kZoomOut:
      return MAKEINTRESOURCE(IDC_ZOOMOUT);
    case ui::mojom::CursorType::kGrab:
      return MAKEINTRESOURCE(IDC_HAND_GRAB);
    case ui::mojom::CursorType::kGrabbing:
      return MAKEINTRESOURCE(IDC_HAND_GRABBING);
    case ui::mojom::CursorType::kNull:
      return IDC_NO;
    case ui::mojom::CursorType::kMiddlePanningVertical:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_VERTICAL);
    case ui::mojom::CursorType::kMiddlePanningHorizontal:
      return MAKEINTRESOURCE(IDC_PAN_MIDDLE_HORIZONTAL);
    // TODO(cef): Find better cursors for these things
    case ui::mojom::CursorType::kDndNone:
      return IDC_ARROW;
    case ui::mojom::CursorType::kDndMove:
      return IDC_ARROW;
    case ui::mojom::CursorType::kDndCopy:
      return IDC_ARROW;
    case ui::mojom::CursorType::kDndLink:
      return IDC_ARROW;
    case ui::mojom::CursorType::kCustom:
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
    ui::mojom::CursorType type) {
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
