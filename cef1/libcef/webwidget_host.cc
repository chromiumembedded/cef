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


void WebWidgetHost::ScheduleComposite() {
  if (invalidate_timer_.IsRunning())
    return;

  // Try to paint at 60fps.
  static int64 kDesiredRate = 16;

  // Maintain the desired rate.
  invalidate_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kDesiredRate),
      this,
      &WebWidgetHost::Invalidate);
}

void WebWidgetHost::ScheduleAnimation() {
  ScheduleComposite();
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

void WebWidgetHost::SchedulePaintTimer() {
  if (layouting_ || paint_timer_.IsRunning())
    return;

  paint_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(0),  // Fire immediately.
      this,
      &WebWidgetHost::DoPaint);
}

void WebWidgetHost::DoPaint() {
 if (MessageLoop::current()->IsIdle()) {
    // Paint to the delegate.
#if defined(OS_MACOSX)
    SkRegion region;
    Paint(region);
#else
    Paint();
#endif
  } else {
    // Try again later.
    SchedulePaintTimer();
  }
}
