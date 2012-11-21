// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BACKING_STORE_OSR_H_
#define CEF_LIBCEF_BROWSER_BACKING_STORE_OSR_H_

#include <vector>

#include "content/browser/renderer_host/backing_store.h"
#include "ui/gfx/canvas.h"

class CefRenderWidgetHostViewOSR;

class BackingStoreOSR : public content::BackingStore {
 public:
  static BackingStoreOSR* From(content::BackingStore* backing_store) {
    return static_cast<BackingStoreOSR*>(backing_store);
  }

  // BackingStore implementation.
  virtual void PaintToBackingStore(
      content::RenderProcessHost* process,
      TransportDIB::Id bitmap,
      const gfx::Rect& bitmap_rect,
      const std::vector<gfx::Rect>& copy_rects,
      float scale_factor,
      const base::Closure& completion_callback,
      bool* scheduled_completion_callback) OVERRIDE;
  virtual bool CopyFromBackingStore(const gfx::Rect& rect,
                                    skia::PlatformBitmap* output) OVERRIDE;
  virtual void ScrollBackingStore(const gfx::Vector2d& delta,
                                  const gfx::Rect& clip_rect,
                                  const gfx::Size& view_size) OVERRIDE;

  const void* getPixels() const;

 private:
  // Can be instantiated only within CefRenderWidgetHostViewOSR.
  friend class CefRenderWidgetHostViewOSR;
  explicit BackingStoreOSR(content::RenderWidgetHost* widget,
      const gfx::Size& size);
  virtual ~BackingStoreOSR() {}

  skia::PlatformBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreOSR);
};

#endif  // CEF_LIBCEF_BROWSER_BACKING_STORE_OSR_H_

