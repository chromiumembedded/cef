// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/backing_store_osr.h"

#include "ui/gfx/rect.h"

BackingStoreOSR::BackingStoreOSR(content::RenderWidgetHost* widget,
                                 const gfx::Size& size)
    : content::BackingStore(widget, size) {
  bitmap_.Allocate(size.width(), size.height(), true);
}

void BackingStoreOSR::ScrollBackingStore(const gfx::Vector2d& delta,
                                         const gfx::Rect& clip_rect,
                                         const gfx::Size& view_size) {
  SkIRect subset_rect = SkIRect::MakeXYWH(clip_rect.x(), clip_rect.y(),
      clip_rect.width(), clip_rect.height());
  bitmap_.GetBitmap().scrollRect(&subset_rect, delta.x(), delta.y());
}

const void* BackingStoreOSR::getPixels() const {
  return const_cast<BackingStoreOSR*>(this)->bitmap_.GetBitmap().getPixels();
}
