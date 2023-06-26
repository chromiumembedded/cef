// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/software_output_device_proxy.h"

#include "base/memory/shared_memory_mapping.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "skia/ext/skia_utils_win.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/win/hwnd_util.h"
#endif

namespace viz {

SoftwareOutputDeviceProxy::~SoftwareOutputDeviceProxy() = default;

SoftwareOutputDeviceProxy::SoftwareOutputDeviceProxy(
    mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater)
    : layered_window_updater_(std::move(layered_window_updater)) {
  DCHECK(layered_window_updater_.is_bound());
}

void SoftwareOutputDeviceProxy::OnSwapBuffers(
    SwapBuffersCallback swap_ack_callback,
    gfx::FrameData data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(swap_ack_callback_.is_null());

  // We aren't waiting on DrawAck() and can immediately run the callback.
  if (!waiting_on_draw_ack_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(swap_ack_callback), viewport_pixel_size_));
    return;
  }

  swap_ack_callback_ =
      base::BindOnce(std::move(swap_ack_callback), viewport_pixel_size_);
}

void SoftwareOutputDeviceProxy::Resize(const gfx::Size& viewport_pixel_size,
                                       float scale_factor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  if (viewport_pixel_size_ == viewport_pixel_size) {
    return;
  }

  viewport_pixel_size_ = viewport_pixel_size;

  canvas_.reset();

  size_t required_bytes;
  if (!ResourceSizes::MaybeSizeInBytes(viewport_pixel_size_,
                                       SinglePlaneFormat::kRGBA_8888,
                                       &required_bytes)) {
    DLOG(ERROR) << "Invalid viewport size " << viewport_pixel_size_.ToString();
    return;
  }

  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(required_bytes);
  if (!region.IsValid()) {
    DLOG(ERROR) << "Failed to allocate " << required_bytes << " bytes";
    return;
  }

#if !BUILDFLAG(IS_WIN)
  auto shm = base::ReadOnlySharedMemoryRegion::Create(required_bytes);
  if (!shm.IsValid()) {
    DLOG(ERROR) << "Failed to allocate " << required_bytes << " bytes";
    return;
  }

  shm_ = region.Map();
  if (!shm_.IsValid()) {
    DLOG(ERROR) << "Failed to map " << required_bytes << " bytes";
    return;
  }

  canvas_ = skia::CreatePlatformCanvasWithPixels(
      viewport_pixel_size_.width(), viewport_pixel_size_.height(), false,
      static_cast<uint8_t*>(shm_.memory()), skia::CRASH_ON_FAILURE);
#else
  canvas_ = skia::CreatePlatformCanvasWithSharedSection(
      viewport_pixel_size_.width(), viewport_pixel_size_.height(), false,
      region.GetPlatformHandle(), skia::CRASH_ON_FAILURE);
#endif

  // Transfer region ownership to the browser process.
  layered_window_updater_->OnAllocatedSharedMemory(viewport_pixel_size_,
                                                   std::move(region));
}

SkCanvas* SoftwareOutputDeviceProxy::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!in_paint_);

  damage_rect_ = damage_rect;
  in_paint_ = true;

  return canvas_.get();
}

void SoftwareOutputDeviceProxy::EndPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(in_paint_);
  DCHECK(!waiting_on_draw_ack_);

  in_paint_ = false;

  gfx::Rect intersected_damage_rect = damage_rect_;
  intersected_damage_rect.Intersect(gfx::Rect(viewport_pixel_size_));
  if (intersected_damage_rect.IsEmpty()) {
    return;
  }

  if (!canvas_) {
    return;
  }

  layered_window_updater_->Draw(
      damage_rect_, base::BindOnce(&SoftwareOutputDeviceProxy::DrawAck,
                                   base::Unretained(this)));
  waiting_on_draw_ack_ = true;

  TRACE_EVENT_ASYNC_BEGIN0("viz", "SoftwareOutputDeviceProxy::Draw", this);
}

void SoftwareOutputDeviceProxy::DrawAck() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(waiting_on_draw_ack_);
  DCHECK(!swap_ack_callback_.is_null());

  TRACE_EVENT_ASYNC_END0("viz", "SoftwareOutputDeviceProxy::Draw", this);

  waiting_on_draw_ack_ = false;
  std::move(swap_ack_callback_).Run();
}

}  // namespace viz
