// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"

#include "content/public/browser/render_process_host.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

BackingStoreOSR::BackingStoreOSR(content::RenderWidgetHost* widget,
                                 const gfx::Size& size)
    : content::BackingStore(widget, size),
      device_(SkBitmap::kARGB_8888_Config, size.width(), size.height(), true),
      canvas_(&device_) {
  canvas_.drawColor(SK_ColorWHITE);
}

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

  SkBitmap src_bitmap;
  src_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                       bitmap_rect.width(),
                       bitmap_rect.height());
  src_bitmap.setPixels(dib->memory());

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  for (size_t i = 0; i < copy_rects.size(); i++) {
    SkIRect src_rect = SkIRect::MakeXYWH(copy_rects[i].x() - bitmap_rect.x(),
                                         copy_rects[i].y() - bitmap_rect.y(),
                                         copy_rects[i].width(),
                                         copy_rects[i].height());
    SkRect paint_rect = SkRect::MakeXYWH(copy_rects[i].x(),
                                         copy_rects[i].y(),
                                         copy_rects[i].width(),
                                         copy_rects[i].height());
    canvas_.drawBitmapRect(src_bitmap, &src_rect, paint_rect, &copy_paint);
  }
  src_bitmap.setPixels(0);
}

bool BackingStoreOSR::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformBitmap* output) {
  if (!output->Allocate(rect.width(), rect.height(), true))
    return false;

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkCanvas canvas(output->GetBitmap());
  canvas.drawColor(SK_ColorWHITE);
  canvas.drawBitmap(device_.accessBitmap(false), 0, 0, &copy_paint);
  return true;
}

void BackingStoreOSR::ScrollBackingStore(const gfx::Vector2d& delta,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  SkIRect subset_rect = SkIRect::MakeXYWH(clip_rect.x(),
                                          clip_rect.y(),
                                          clip_rect.width(),
                                          clip_rect.height());
  device_.accessBitmap(true).scrollRect(&subset_rect, delta.x(), delta.y());
}

const void* BackingStoreOSR::getPixels() const {
  return const_cast<BackingStoreOSR*>(this)->device_.
      accessBitmap(false).getPixels();
}
