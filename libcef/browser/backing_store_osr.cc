// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"

#include <algorithm>

#include "content/browser/renderer_host/dip_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"
#include "ui/surface/transport_dib.h"

// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

BackingStoreOSR::BackingStoreOSR(content::RenderWidgetHost* widget,
                                 const gfx::Size& size,
                                 float scale_factor)
    : content::BackingStore(widget, size),
      device_scale_factor_(scale_factor) {
  gfx::Size pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size, device_scale_factor_));
  device_.reset(new SkBitmapDevice(SkBitmap::kARGB_8888_Config,
                                   pixel_size.width(),
                                   pixel_size.height(),
                                   true));
  canvas_.reset(new SkCanvas(device_.get()));

  canvas_->drawColor(SK_ColorWHITE);
}

void BackingStoreOSR::ScaleFactorChanged(float scale_factor) {
  if (scale_factor == device_scale_factor_)
    return;

  gfx::Size old_pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_));
  device_scale_factor_ = scale_factor;

  gfx::Size pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_));

  scoped_ptr<SkBaseDevice> new_device(
      new SkBitmapDevice(SkBitmap::kARGB_8888_Config,
                         pixel_size.width(),
                         pixel_size.height(),
                         true));

  scoped_ptr<SkCanvas> new_canvas(new SkCanvas(new_device.get()));

  // Copy old contents; a low-res flash is better than a black flash.
  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  SkIRect src_rect = SkIRect::MakeWH(old_pixel_size.width(),
                                     old_pixel_size.height());
  SkRect dst_rect = SkRect::MakeWH(pixel_size.width(), pixel_size.height());
  new_canvas.get()->drawBitmapRect(device_->accessBitmap(false), &src_rect,
                                   dst_rect, &copy_paint);

  canvas_.swap(new_canvas);
  device_.swap(new_device);
}

size_t BackingStoreOSR::MemorySize() {
  // NOTE: The computation may be different when the canvas is a subrectangle of
  // a larger bitmap.
  return gfx::ToFlooredSize(
      gfx::ScaleSize(size(), device_scale_factor_)).GetArea() * 4;
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
  if (bitmap_rect.IsEmpty())
    return;

  gfx::Rect pixel_bitmap_rect = gfx::ToEnclosedRect(
      gfx::ScaleRect(bitmap_rect, scale_factor));

  const int width = pixel_bitmap_rect.width();
  const int height = pixel_bitmap_rect.height();

  if (width <= 0 || width > kMaxVideoLayerSize ||
      height <= 0 || height > kMaxVideoLayerSize) {
    return;
  }

  TransportDIB* dib = process->GetTransportDIB(bitmap);
  if (!dib)
    return;

  SkBitmap src_bitmap;
  src_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  src_bitmap.setPixels(dib->memory());

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  for (size_t i = 0; i < copy_rects.size(); i++) {
    const gfx::Rect pixel_copy_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(copy_rects[i], scale_factor));
    SkIRect src_rect =
        SkIRect::MakeXYWH(pixel_copy_rect.x() - pixel_bitmap_rect.x(),
                          pixel_copy_rect.y() - pixel_bitmap_rect.y(),
                          pixel_copy_rect.width(),
                          pixel_copy_rect.height());

    const gfx::Rect pixel_copy_dst_rect = gfx::ToEnclosingRect(
        gfx::ScaleRect(copy_rects[i], device_scale_factor_));
    SkRect paint_rect = SkRect::MakeXYWH(pixel_copy_dst_rect.x(),
                                         pixel_copy_dst_rect.y(),
                                         pixel_copy_dst_rect.width(),
                                         pixel_copy_dst_rect.height());
    canvas_->drawBitmapRect(src_bitmap, &src_rect, paint_rect, &copy_paint);
  }
  src_bitmap.setPixels(0);
}

bool BackingStoreOSR::CopyFromBackingStore(const gfx::Rect& rect,
                                           skia::PlatformBitmap* output) {
  const int width =
      std::min(size().width(), rect.width()) * device_scale_factor_;
  const int height =
      std::min(size().height(), rect.height()) * device_scale_factor_;
  if (!output->Allocate(width, height, true))
    return false;

  SkPaint copy_paint;
  copy_paint.setXfermodeMode(SkXfermode::kSrc_Mode);

  SkCanvas canvas(output->GetBitmap());
  canvas.drawColor(SK_ColorWHITE);
  canvas.drawBitmap(device_->accessBitmap(false), 0, 0, &copy_paint);
  return true;
}

void BackingStoreOSR::ScrollBackingStore(const gfx::Vector2d& delta,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  gfx::Rect pixel_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(clip_rect, device_scale_factor_));
  gfx::Vector2d pixel_delta = gfx::ToFlooredVector2d(
      gfx::ScaleVector2d(delta, device_scale_factor_));

  int x = std::min(pixel_rect.x(), pixel_rect.x() - pixel_delta.x());
  int y = std::min(pixel_rect.y(), pixel_rect.y() - pixel_delta.y());
  int w = pixel_rect.width() + abs(pixel_delta.x());
  int h = pixel_rect.height() + abs(pixel_delta.y());
  SkIRect rect = SkIRect::MakeXYWH(x, y, w, h);

  device_->accessBitmap(true).scrollRect(&rect,
                                         pixel_delta.x(),
                                         pixel_delta.y());
}

const void* BackingStoreOSR::getPixels() const {
  return const_cast<BackingStoreOSR*>(this)->device_->
      accessBitmap(false).getPixels();
}
