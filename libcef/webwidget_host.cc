// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/webwidget_host.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

using webkit::npapi::WebPluginGeometry;
using WebKit::WebSize;


void WebWidgetHost::ScheduleComposite() {
  if (has_invalidate_task_)
    return;

  has_invalidate_task_ = true;

  // Try to paint at 60fps.
  static int64 kDesiredRate = 16;

  base::TimeDelta delta = base::TimeTicks::Now() - paint_last_call_;
  int64 actualRate = delta.InMilliseconds();
  if (actualRate >= kDesiredRate) {
    // Can't keep up so run as fast as possible.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&WebWidgetHost::Invalidate, weak_factory_.GetWeakPtr()));
  } else {
    // Maintain the desired rate.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(&WebWidgetHost::Invalidate, weak_factory_.GetWeakPtr()),
        kDesiredRate - actualRate);
  }
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

void WebWidgetHost::DoPaint() {
  // TODO(cef): The below code is cross-platform but the IsIdle() method
  // currently requires patches to Chromium. Since this code is only executed
  // on Windows it's been stuck behind an #ifdef for now to avoid having to
  // patch Chromium code on other platforms.
#if defined(OS_WIN)
  if (MessageLoop::current()->IsIdle()) {
    has_update_task_ = false;
    // Paint to the delegate.
    Paint();
  } else {
    // Try again later.
    CefThread::PostTask(CefThread::UI, FROM_HERE,
        base::Bind(&WebWidgetHost::DoPaint, weak_factory_.GetWeakPtr()));
  }
#else
  NOTIMPLEMENTED();
#endif
}
