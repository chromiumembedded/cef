// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/webwidget_host.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/glue/webkit_glue.h"

using webkit::npapi::WebPluginGeometry;
using WebKit::WebSize;

const int WebWidgetHost::kDefaultFrameRate = 30;
const int WebWidgetHost::kMaxFrameRate = 90;

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect) {
  if (rect.IsEmpty())
    return;

  int width, height;
  GetSize(width, height);
  gfx::Rect client_rect(width, height);
  client_rect.Intersect(rect);
  if (client_rect.IsEmpty())
    return;

  UpdatePaintRect(client_rect);

  if (view_)
    InvalidateWindowRect(client_rect);
  else
    ScheduleTimer();
}

void WebWidgetHost::ScheduleComposite() {
  ScheduleTimer();
}

void WebWidgetHost::ScheduleAnimation() {
  ScheduleTimer();
}

bool WebWidgetHost::GetImage(int width, int height, void* rgba_buffer) {
  if (!canvas_.get())
    return false;

  const SkBitmap& bitmap = canvas_->getDevice()->accessBitmap(false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  if (width == canvas_->getDevice()->width() &&
      height == canvas_->getDevice()->height()) {
    // The specified width and height values are the same as the canvas size.
    // Return the existing canvas contents.
    const void* pixels = bitmap.getPixels();
    memcpy(rgba_buffer, pixels, width * height * 4);
    return true;
  }

  // Create a new canvas of the requested size.
  scoped_ptr<skia::PlatformCanvas> new_canvas(
      skia::CreatePlatformCanvas(width, height, true));

  new_canvas->writePixels(bitmap, 0, 0);
  const SkBitmap& new_bitmap = new_canvas->getDevice()->accessBitmap(false);
  DCHECK(new_bitmap.config() == SkBitmap::kARGB_8888_Config);

  // Return the new canvas contents.
  const void* pixels = new_bitmap.getPixels();
  memcpy(rgba_buffer, pixels, width * height * 4);
  return true;
}

void WebWidgetHost::SetSize(int width, int height) {
  webwidget_->resize(WebSize(width, height));
  InvalidateRect(gfx::Rect(0, 0, width, height));
  EnsureTooltip();
}

void WebWidgetHost::GetSize(int& width, int& height) {
  const WebSize& size = webwidget_->size();
  width = size.width;
  height = size.height;
}

void WebWidgetHost::AddWindowedPlugin(gfx::PluginWindowHandle handle) {
  WebPluginGeometry geometry;
  plugin_map_.insert(std::make_pair(handle, geometry));
}

void WebWidgetHost::RemoveWindowedPlugin(gfx::PluginWindowHandle handle) {
  PluginMap::iterator it = plugin_map_.find(handle);
  DCHECK(it != plugin_map_.end());
  plugin_map_.erase(it);
}

void WebWidgetHost::MoveWindowedPlugin(const WebPluginGeometry& move) {
  PluginMap::iterator it = plugin_map_.find(move.window);
  DCHECK(it != plugin_map_.end());

  it->second.window = move.window;
  if (move.rects_valid) {
    it->second.window_rect = move.window_rect;
    it->second.clip_rect = move.clip_rect;
    it->second.cutout_rects = move.cutout_rects;
    it->second.rects_valid = true;
  }
  it->second.visible = move.visible;
}

gfx::PluginWindowHandle WebWidgetHost::GetWindowedPluginAt(int x, int y) {
  if (!plugin_map_.empty()) {
    PluginMap::const_iterator it = plugin_map_.begin();
    for (; it != plugin_map_.end(); ++it) {
      if (it->second.visible && it->second.window_rect.Contains(x, y))
        return it->second.window;
    }
  }

  return gfx::kNullPluginWindow;
}

void WebWidgetHost::SetFrameRate(int frames_per_second) {
  if (frames_per_second <= 0)
    frames_per_second = kDefaultFrameRate;
  if (frames_per_second > kMaxFrameRate)
    frames_per_second = kMaxFrameRate;

  frame_delay_ = 1000 / frames_per_second;
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  paint_rgn_.op(rect.x(), rect.y(), rect.right(), rect.bottom(),
      SkRegion::kUnion_Op);
#else
  // TODO(cef): Update all ports to use regions instead of rectangles.
  paint_rect_.Union(rect);
#endif
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
#ifndef NDEBUG
  DCHECK(!painting_);
#endif
  DCHECK(canvas_.get());

  if (rect.IsEmpty())
    return;

  if (IsTransparent()) {
    // When using transparency mode clear the rectangle before painting.
    SkPaint clearpaint;
    clearpaint.setARGB(0, 0, 0, 0);
    clearpaint.setXfermodeMode(SkXfermode::kClear_Mode);

    SkRect skrc;
    skrc.set(rect.x(), rect.y(), rect.right(), rect.bottom());
    canvas_->drawRect(skrc, clearpaint);
  }

  set_painting(true);
  webwidget_->paint(webkit_glue::ToWebCanvas(canvas_.get()), rect);
  set_painting(false);
}

void WebWidgetHost::ScheduleTimer() {
  if (timer_.IsRunning())
    return;

  // This method may be called multiple times while the timer callback is
  // executing. If so re-execute this method a single time after the callback
  // has completed.
  if (timer_executing_) {
    if (!timer_wanted_)
      timer_wanted_ = true;
    return;
  }

  // Maintain the desired rate.
  base::TimeDelta delta = base::TimeTicks::Now() - timer_last_;
  int64 actualRate = delta.InMilliseconds();
  if (actualRate >= frame_delay_)
    delta = base::TimeDelta::FromMilliseconds(1);
  else
    delta = base::TimeDelta::FromMilliseconds(frame_delay_ - actualRate);

  timer_.Start(
      FROM_HERE,
      delta,
      this,
      &WebWidgetHost::DoTimer);
}

void WebWidgetHost::DoTimer() {
  timer_executing_ = true;

  if (view_) {
    // Window rendering is enabled and we've received a requestAnimationFrame
    // or similar call. Trigger the OS to invalidate/repaint the client area at
    // the requested frequency.
    InvalidateWindow();
  } else {
    // Window rendering is disabled. Generate OnPaint() calls at the requested
    // frequency.
#if defined(OS_MACOSX)
    SkRegion region;
    Paint(region);
#else
    Paint();
#endif
  }

  timer_executing_ = false;

  timer_last_ = base::TimeTicks::Now();

  if (timer_wanted_) {
    timer_wanted_ = false;
    ScheduleTimer();
  }
}
