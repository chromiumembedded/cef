// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#undef Status  // Avoid conflicts with url_request_status.h

#include "libcef/browser/native/window_x11.h"

#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"
#endif  // defined(USE_X11)

#if defined(USE_X11)
namespace {

// Based on ui/base/cursor/cursor_loader_x11.cc.

int ToCursorID(ui::mojom::CursorType type) {
  switch (type) {
    case ui::mojom::CursorType::kPointer:
      return XC_left_ptr;
    case ui::mojom::CursorType::kCross:
      return XC_crosshair;
    case ui::mojom::CursorType::kHand:
      return XC_hand2;
    case ui::mojom::CursorType::kIBeam:
      return XC_xterm;
    case ui::mojom::CursorType::kWait:
      return XC_watch;
    case ui::mojom::CursorType::kHelp:
      return XC_question_arrow;
    case ui::mojom::CursorType::kEastResize:
      return XC_right_side;
    case ui::mojom::CursorType::kNorthResize:
      return XC_top_side;
    case ui::mojom::CursorType::kNorthEastResize:
      return XC_top_right_corner;
    case ui::mojom::CursorType::kNorthWestResize:
      return XC_top_left_corner;
    case ui::mojom::CursorType::kSouthResize:
      return XC_bottom_side;
    case ui::mojom::CursorType::kSouthEastResize:
      return XC_bottom_right_corner;
    case ui::mojom::CursorType::kSouthWestResize:
      return XC_bottom_left_corner;
    case ui::mojom::CursorType::kWestResize:
      return XC_left_side;
    case ui::mojom::CursorType::kNorthSouthResize:
      return XC_sb_v_double_arrow;
    case ui::mojom::CursorType::kEastWestResize:
      return XC_sb_h_double_arrow;
    case ui::mojom::CursorType::kNorthEastSouthWestResize:
      return XC_left_ptr;
    case ui::mojom::CursorType::kNorthWestSouthEastResize:
      return XC_left_ptr;
    case ui::mojom::CursorType::kColumnResize:
      return XC_sb_h_double_arrow;
    case ui::mojom::CursorType::kRowResize:
      return XC_sb_v_double_arrow;
    case ui::mojom::CursorType::kMiddlePanning:
      return XC_fleur;
    case ui::mojom::CursorType::kEastPanning:
      return XC_sb_right_arrow;
    case ui::mojom::CursorType::kNorthPanning:
      return XC_sb_up_arrow;
    case ui::mojom::CursorType::kNorthEastPanning:
      return XC_top_right_corner;
    case ui::mojom::CursorType::kNorthWestPanning:
      return XC_top_left_corner;
    case ui::mojom::CursorType::kSouthPanning:
      return XC_sb_down_arrow;
    case ui::mojom::CursorType::kSouthEastPanning:
      return XC_bottom_right_corner;
    case ui::mojom::CursorType::kSouthWestPanning:
      return XC_bottom_left_corner;
    case ui::mojom::CursorType::kWestPanning:
      return XC_sb_left_arrow;
    case ui::mojom::CursorType::kMove:
      return XC_fleur;
    case ui::mojom::CursorType::kVerticalText:
      return XC_left_ptr;
    case ui::mojom::CursorType::kCell:
      return XC_left_ptr;
    case ui::mojom::CursorType::kContextMenu:
      return XC_left_ptr;
    case ui::mojom::CursorType::kAlias:
      return XC_left_ptr;
    case ui::mojom::CursorType::kProgress:
      return XC_left_ptr;
    case ui::mojom::CursorType::kNoDrop:
      return XC_left_ptr;
    case ui::mojom::CursorType::kCopy:
      return XC_left_ptr;
    case ui::mojom::CursorType::kNotAllowed:
      return XC_left_ptr;
    case ui::mojom::CursorType::kZoomIn:
      return XC_left_ptr;
    case ui::mojom::CursorType::kZoomOut:
      return XC_left_ptr;
    case ui::mojom::CursorType::kGrab:
      return XC_left_ptr;
    case ui::mojom::CursorType::kGrabbing:
      return XC_left_ptr;
    case ui::mojom::CursorType::kMiddlePanningVertical:
      return XC_left_ptr;
    case ui::mojom::CursorType::kMiddlePanningHorizontal:
      return XC_left_ptr;
    case ui::mojom::CursorType::kDndNone:
      return XC_left_ptr;
    case ui::mojom::CursorType::kDndMove:
      return XC_left_ptr;
    case ui::mojom::CursorType::kDndCopy:
      return XC_left_ptr;
    case ui::mojom::CursorType::kDndLink:
      return XC_left_ptr;
    case ui::mojom::CursorType::kNull:
      return XC_left_ptr;
    case ui::mojom::CursorType::kCustom:
    case ui::mojom::CursorType::kNone:
      break;
  }
  NOTREACHED();
  return 0;
}

// The following XCursorCache code was deleted from ui/base/x/x11_util.cc in
// https://crbug.com/665574#c2

// A process wide singleton that manages the usage of X cursors.
class XCursorCache {
 public:
  XCursorCache() {}
  ~XCursorCache() { Clear(); }

  ::Cursor GetCursor(int cursor_shape) {
    // Lookup cursor by attempting to insert a null value, which avoids
    // a second pass through the map after a cache miss.
    std::pair<std::map<int, ::Cursor>::iterator, bool> it =
        cache_.insert(std::make_pair(cursor_shape, 0));
    if (it.second) {
      XDisplay* display = gfx::GetXDisplay();
      it.first->second = XCreateFontCursor(display, cursor_shape);
    }
    return it.first->second;
  }

  void Clear() {
    XDisplay* display = gfx::GetXDisplay();
    for (std::map<int, ::Cursor>::iterator it = cache_.begin();
         it != cache_.end(); ++it) {
      XFreeCursor(display, it->second);
    }
    cache_.clear();
  }

 private:
  // Maps X11 font cursor shapes to Cursor IDs.
  std::map<int, ::Cursor> cache_;

  DISALLOW_COPY_AND_ASSIGN(XCursorCache);
};

XCursorCache* cursor_cache = nullptr;

// Returns an X11 Cursor, sharable across the process.
// |cursor_shape| is an X font cursor shape, see XCreateFontCursor().
::Cursor GetXCursor(int cursor_shape) {
  if (!cursor_cache)
    cursor_cache = new XCursorCache;
  return cursor_cache->GetCursor(cursor_shape);
}

}  // namespace
#endif  // defined(USE_X11)

ui::PlatformCursor CefRenderWidgetHostViewOSR::GetPlatformCursor(
    ui::mojom::CursorType type) {
#if defined(USE_X11)
  if (type == ui::mojom::CursorType::kNone) {
    if (!invisible_cursor_) {
      invisible_cursor_.reset(new ui::XScopedCursor(ui::CreateInvisibleCursor(),
                                                    gfx::GetXDisplay()));
    }
    return invisible_cursor_->get();
  } else {
    return GetXCursor(ToCursorID(type));
  }
#endif  // defined(USE_X11)
  return 0;
}
