// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webwidget_host.h"
#include "cef_thread.h"

#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"

using webkit::npapi::WebPluginGeometry;
using WebKit::WebSize;


void WebWidgetHost::ScheduleAnimation() {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      factory_.NewRunnableMethod(&WebWidgetHost::ScheduleComposite), 10);
}

void WebWidgetHost::DiscardBackingStore() {
  canvas_.reset();
}

void WebWidgetHost::UpdatePaintRect(const gfx::Rect& rect) {
  paint_rect_ = paint_rect_.Union(rect);
}

void WebWidgetHost::SetSize(int width, int height) {
  // Force an entire re-paint.  TODO(darin): Maybe reuse this memory buffer.
  DiscardBackingStore();

  webwidget_->resize(WebSize(width, height));
  EnsureTooltip();
}

void WebWidgetHost::GetSize(int& width, int& height) {
  const WebSize& size = webwidget_->size();
  width = size.width;
  height = size.height;
}

void WebWidgetHost::AddWindowedPlugin(gfx::PluginWindowHandle handle)
{
  WebPluginGeometry geometry;
  plugin_map_.insert(std::make_pair(handle, geometry));
}

void WebWidgetHost::RemoveWindowedPlugin(gfx::PluginWindowHandle handle)
{
  PluginMap::iterator it = plugin_map_.find(handle);
  DCHECK(it != plugin_map_.end());
  plugin_map_.erase(it);
}

void WebWidgetHost::MoveWindowedPlugin(const WebPluginGeometry& move)
{
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

gfx::PluginWindowHandle WebWidgetHost::GetWindowedPluginAt(int x, int y)
{
  if (!plugin_map_.empty()) {
    PluginMap::const_iterator it = plugin_map_.begin();
    for(; it != plugin_map_.end(); ++it) {
      if (it->second.visible && it->second.window_rect.Contains(x, y))
        return it->second.window;
    }
  }

  return NULL;
}

void WebWidgetHost::DoPaint() {
  update_task_ = NULL;

  if (update_rect_.IsEmpty())
    return;

  // TODO(cef): The below code is cross-platform but the IsIdle() method
  // currently requires patches to Chromium. Since this code is only executed
  // on Windows it's been stuck behind an #ifdef for now to avoid having to
  // patch Chromium code on other platforms.
#if defined(OS_WIN)
  if (MessageLoop::current()->IsIdle()) {
    // Perform the paint.
    UpdatePaintRect(update_rect_);
    update_rect_ = gfx::Rect();
    Paint();
  } else {
    // Try again later.
    update_task_ = factory_.NewRunnableMethod(&WebWidgetHost::DoPaint);
    CefThread::PostTask(CefThread::UI, FROM_HERE, update_task_);
  }
#else
  NOTIMPLEMENTED();
#endif
}
