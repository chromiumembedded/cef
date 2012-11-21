// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"

#include "content/public/browser/render_process_host.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

// Portions extracted from content/browser/renderer_host/backing_store_win.cc.

namespace {

void CallStretchDIBits(HDC hdc, int dest_x, int dest_y, int dest_w, int dest_h,
                      int src_x, int src_y, int src_w, int src_h, void* pixels,
                      const BITMAPINFO* bitmap_info) {
  // When blitting a rectangle that touches the bottom left corner of the bitmap
  // StretchDIBits looks at it top-down! For more details, see
  // http://wiki.allegro.cc/index.php?title=StretchDIBits.
  int rv;
  int bitmap_h = -bitmap_info->bmiHeader.biHeight;
  int bottom_up_src_y = bitmap_h - src_y - src_h;
  if (bottom_up_src_y == 0 && src_x == 0 && src_h != bitmap_h) {
    rv = StretchDIBits(hdc,
                       dest_x, dest_h + dest_y - 1, dest_w, -dest_h,
                       src_x, bitmap_h - src_y + 1, src_w, -src_h,
                       pixels, bitmap_info, DIB_RGB_COLORS, SRCCOPY);
  } else {
    rv = StretchDIBits(hdc,
                       dest_x, dest_y, dest_w, dest_h,
                       src_x, bottom_up_src_y, src_w, src_h,
                       pixels, bitmap_info, DIB_RGB_COLORS, SRCCOPY);
  }
  DCHECK(rv != GDI_ERROR);
}

}  // namespace

void BackingStoreOSR::PaintToBackingStore(
    content::RenderProcessHost* process,
    TransportDIB::Id bitmap,
    const gfx::Rect& bitmap_rect,
    const std::vector<gfx::Rect>& copy_rects,
    float scale_factor,
    const base::Closure& completion_callback,
    bool* scheduled_completion_callback) {
  *scheduled_completion_callback = false;
  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  BITMAPINFOHEADER hdr;
  gfx::CreateBitmapHeader(bitmap_rect.width(), bitmap_rect.height(), &hdr);

  // Account for a bitmap_rect that exceeds the bounds of our view.
  gfx::Rect view_rect(size());
  HDC temp_dc = bitmap_.GetSurface();
  for (size_t i = 0; i < copy_rects.size(); i++) {
    gfx::Rect paint_rect(copy_rects[i]);
    paint_rect.Intersect(view_rect);
    CallStretchDIBits(temp_dc,
                      paint_rect.x(),
                      paint_rect.y(),
                      paint_rect.width(),
                      paint_rect.height(),
                      paint_rect.x() - bitmap_rect.x(),
                      paint_rect.y() - bitmap_rect.y(),
                      paint_rect.width(),
                      paint_rect.height(),
                      dib->memory(),
                      reinterpret_cast<BITMAPINFO*>(&hdr));
  }
}

bool BackingStoreOSR::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformBitmap* output) {
  if (!output->Allocate(rect.width(), rect.height(), true))
    return false;

  HDC src_dc = bitmap_.GetSurface();
  HDC dst_dc = output->GetSurface();
  return BitBlt(dst_dc, 0, 0, rect.width(), rect.height(),
                src_dc, rect.x(), rect.y(), SRCCOPY) ? true : false;
}
