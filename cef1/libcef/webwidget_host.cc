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

using webkit::npapi::WebPluginGeometry;
using WebKit::WebSize;

const int WebWidgetHost::kDefaultFrameRate = 30;
const int WebWidgetHost::kMaxFrameRate = 90;

void WebWidgetHost::ScheduleComposite() {
  ScheduleInvalidateTimer();
}

void WebWidgetHost::ScheduleAnimation() {
  ScheduleInvalidateTimer();
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  paint_rgn_.op(rect.x(), rect.y(), rect.right(), rect.bottom(),
      SkRegion::kUnion_Op);
#else
  // TODO(cef): Update all ports to use regions instead of rectangles.
  paint_rect_ = paint_rect_.Union(rect);
#endif
}

void WebWidgetHost::UpdateRedrawRect(const gfx::Rect& rect) {
  if (!view_)
    redraw_rect_ = redraw_rect_.Union(rect);
}

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect) {
  if (rect.IsEmpty())
    return;
  DidInvalidateRect(rect);
}

void WebWidgetHost::SetSize(int width, int height) {
  webwidget_->resize(WebSize(width, height));
  DidInvalidateRect(gfx::Rect(0, 0, width, height));
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

void WebWidgetHost::ScheduleInvalidateTimer() {
  if (invalidate_timer_.IsRunning())
    return;

  // Maintain the desired rate.
  base::TimeDelta delta = base::TimeTicks::Now() - last_invalidate_time_;
  int64 actualRate = delta.InMilliseconds();
  if (actualRate >= frame_delay_)
    delta = base::TimeDelta::FromMilliseconds(1);
  else
    delta = base::TimeDelta::FromMilliseconds(frame_delay_ - actualRate);

  invalidate_timer_.Start(
      FROM_HERE,
      delta,
      this,
      &WebWidgetHost::DoInvalidate);
}

void WebWidgetHost::DoInvalidate() {
  if (!webwidget_)
    return;
  WebSize size = webwidget_->size();
  InvalidateRect(gfx::Rect(0, 0, size.width, size.height));

  last_invalidate_time_ = base::TimeTicks::Now();
}

void WebWidgetHost::SchedulePaintTimer() {
  if (layouting_ || paint_timer_.IsRunning())
    return;

  // Maintain the desired rate.
  base::TimeDelta delta = base::TimeTicks::Now() - last_paint_time_;
  int64 actualRate = delta.InMilliseconds();
  if (actualRate >= frame_delay_)
    delta = base::TimeDelta::FromMilliseconds(1);
  else
    delta = base::TimeDelta::FromMilliseconds(frame_delay_ - actualRate);

  paint_timer_.Start(
      FROM_HERE,
      delta,
      this,
      &WebWidgetHost::DoPaint);
}

void WebWidgetHost::DoPaint() {
#if defined(OS_MACOSX)
  SkRegion region;
  Paint(region);
#else
  Paint();
#endif

  last_paint_time_ = base::TimeTicks::Now();
}
