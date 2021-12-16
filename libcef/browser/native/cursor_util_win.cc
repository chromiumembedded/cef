// Copyright 2020 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/native/cursor_util.h"

#include <windows.h>

#include "libcef/common/app_manager.h"

#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/win/win_cursor.h"
#include "ui/resources/grit/ui_unscaled_resources.h"

namespace cursor_util {

namespace {

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
    case ui::mojom::CursorType::kNorthSouthNoResize:
    case ui::mojom::CursorType::kNorthSouthResize:
      return IDC_SIZENS;
    case ui::mojom::CursorType::kEastWestNoResize:
    case ui::mojom::CursorType::kEastWestResize:
      return IDC_SIZEWE;
    case ui::mojom::CursorType::kNorthEastSouthWestNoResize:
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case ui::mojom::CursorType::kNorthWestSouthEastNoResize:
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
    case ui::mojom::CursorType::kAlias:
      return MAKEINTRESOURCE(IDC_ALIAS);
    case ui::mojom::CursorType::kProgress:
      return IDC_APPSTARTING;
    case ui::mojom::CursorType::kNoDrop:
      return IDC_NO;
    case ui::mojom::CursorType::kCopy:
      return MAKEINTRESOURCE(IDC_COPYCUR);
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
    case ui::mojom::CursorType::kContextMenu:
    case ui::mojom::CursorType::kCustom:
    case ui::mojom::CursorType::kNone:
      NOTIMPLEMENTED();
      return IDC_ARROW;
  }
  NOTREACHED();
  return NULL;
}

bool IsSystemCursorID(LPCWSTR cursor_id) {
  return cursor_id >= IDC_ARROW;  // See WinUser.h
}

}  // namespace

cef_cursor_handle_t GetPlatformCursor(ui::mojom::CursorType type) {
  // Using a dark 1x1 bit bmp kNone cursor may still cause DWM to do composition
  // work unnecessarily. Better to totally remove it from the screen.
  // crbug.com/1069698
  if (type == ui::mojom::CursorType::kNone) {
    return nullptr;
  }

  HMODULE module_handle = NULL;
  const wchar_t* cursor_id = ToCursorID(type);
  if (!IsSystemCursorID(cursor_id)) {
    module_handle =
        ::GetModuleHandle(CefAppManager::Get()->GetResourceDllName());
    if (!module_handle)
      module_handle = ::GetModuleHandle(NULL);
  }

  return LoadCursor(module_handle, cursor_id);
}

cef_cursor_handle_t ToCursorHandle(scoped_refptr<ui::PlatformCursor> cursor) {
  return ui::WinCursor::FromPlatformCursor(cursor)->hcursor();
}

}  // namespace cursor_util
