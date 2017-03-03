// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#undef Status  // Avoid conflicts with url_request_status.h

#include "libcef/browser/native/window_x11.h"

#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_types.h"

namespace {

// Based on ui/base/cursor/cursor_loader_x11.cc.

using blink::WebCursorInfo;

int ToCursorID(WebCursorInfo::Type type) {
  switch (type) {
    case WebCursorInfo::TypePointer:
      return XC_left_ptr;
    case WebCursorInfo::TypeCross:
      return XC_crosshair;
    case WebCursorInfo::TypeHand:
      return XC_hand2;
    case WebCursorInfo::TypeIBeam:
      return XC_xterm;
    case WebCursorInfo::TypeWait:
      return XC_watch;
    case WebCursorInfo::TypeHelp:
      return XC_question_arrow;
    case WebCursorInfo::TypeEastResize:
      return XC_right_side;
    case WebCursorInfo::TypeNorthResize:
      return XC_top_side;
    case WebCursorInfo::TypeNorthEastResize:
      return XC_top_right_corner;
    case WebCursorInfo::TypeNorthWestResize:
      return XC_top_left_corner;
    case WebCursorInfo::TypeSouthResize:
      return XC_bottom_side;
    case WebCursorInfo::TypeSouthEastResize:
      return XC_bottom_right_corner;
    case WebCursorInfo::TypeSouthWestResize:
      return XC_bottom_left_corner;
    case WebCursorInfo::TypeWestResize:
      return XC_left_side;
    case WebCursorInfo::TypeNorthSouthResize:
      return XC_sb_v_double_arrow;
    case WebCursorInfo::TypeEastWestResize:
      return XC_sb_h_double_arrow;
    case WebCursorInfo::TypeNorthEastSouthWestResize:
      return XC_left_ptr;
    case WebCursorInfo::TypeNorthWestSouthEastResize:
      return XC_left_ptr;
    case WebCursorInfo::TypeColumnResize:
      return XC_sb_h_double_arrow;
    case WebCursorInfo::TypeRowResize:
      return XC_sb_v_double_arrow;
    case WebCursorInfo::TypeMiddlePanning:
      return XC_fleur;
    case WebCursorInfo::TypeEastPanning:
      return XC_sb_right_arrow;
    case WebCursorInfo::TypeNorthPanning:
      return XC_sb_up_arrow;
    case WebCursorInfo::TypeNorthEastPanning:
      return XC_top_right_corner;
    case WebCursorInfo::TypeNorthWestPanning:
      return XC_top_left_corner;
    case WebCursorInfo::TypeSouthPanning:
      return XC_sb_down_arrow;
    case WebCursorInfo::TypeSouthEastPanning:
      return XC_bottom_right_corner;
    case WebCursorInfo::TypeSouthWestPanning:
      return XC_bottom_left_corner;
    case WebCursorInfo::TypeWestPanning:
      return XC_sb_left_arrow;
    case WebCursorInfo::TypeMove:
      return XC_fleur;
    case WebCursorInfo::TypeVerticalText:
      return XC_left_ptr;
    case WebCursorInfo::TypeCell:
      return XC_left_ptr;
    case WebCursorInfo::TypeContextMenu:
      return XC_left_ptr;
    case WebCursorInfo::TypeAlias:
      return XC_left_ptr;
    case WebCursorInfo::TypeProgress:
      return XC_left_ptr;
    case WebCursorInfo::TypeNoDrop:
      return XC_left_ptr;
    case WebCursorInfo::TypeCopy:
      return XC_left_ptr;
    case WebCursorInfo::TypeNotAllowed:
      return XC_left_ptr;
    case WebCursorInfo::TypeZoomIn:
      return XC_left_ptr;
    case WebCursorInfo::TypeZoomOut:
      return XC_left_ptr;
    case WebCursorInfo::TypeGrab:
      return XC_left_ptr;
    case WebCursorInfo::TypeGrabbing:
      return XC_left_ptr;
    case WebCursorInfo::TypeCustom:
    case WebCursorInfo::TypeNone:
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
  ~XCursorCache() {
    Clear();
  }

  ::Cursor GetCursor(int cursor_shape) {
    // Lookup cursor by attempting to insert a null value, which avoids
    // a second pass through the map after a cache miss.
    std::pair<std::map<int, ::Cursor>::iterator, bool> it = cache_.insert(
        std::make_pair(cursor_shape, 0));
    if (it.second) {
      XDisplay* display = gfx::GetXDisplay();
      it.first->second = XCreateFontCursor(display, cursor_shape);
    }
    return it.first->second;
  }

  void Clear() {
    XDisplay* display = gfx::GetXDisplay();
    for (std::map<int, ::Cursor>::iterator it =
        cache_.begin(); it != cache_.end(); ++it) {
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

void CefRenderWidgetHostViewOSR::PlatformCreateCompositorWidget(
    bool is_guest_view_hack) {
  // Create a hidden 1x1 window. It will delete itself on close.
  window_ = new CefWindowX11(NULL, None, gfx::Rect(0, 0, 1, 1));
  compositor_widget_ = window_->xwindow();
}

void CefRenderWidgetHostViewOSR::PlatformResizeCompositorWidget(
    const gfx::Size&) {
}

void CefRenderWidgetHostViewOSR::PlatformDestroyCompositorWidget() {
  DCHECK(window_);
  window_->Close();
  compositor_widget_ = gfx::kNullAcceleratedWidget;
}

ui::PlatformCursor CefRenderWidgetHostViewOSR::GetPlatformCursor(
    blink::WebCursorInfo::Type type) {
  if (type == WebCursorInfo::TypeNone) {
    if (!invisible_cursor_) {
      invisible_cursor_.reset(
          new ui::XScopedCursor(ui::CreateInvisibleCursor(),
                                gfx::GetXDisplay()));
    }
    return invisible_cursor_->get();
  } else {
    return GetXCursor(ToCursorID(type));
  }
}
