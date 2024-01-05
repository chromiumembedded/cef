// Copyright 2019 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/host_display_client_osr.h"

#include <utility>

#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include "base/memory/shared_memory_mapping.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/src/core/SkDevice.h"
#include "ui/gfx/skia_util.h"

#if BUILDFLAG(IS_WIN)
#include "skia/ext/skia_utils_win.h"
#endif

class CefLayeredWindowUpdaterOSR : public viz::mojom::LayeredWindowUpdater {
 public:
  CefLayeredWindowUpdaterOSR(
      CefRenderWidgetHostViewOSR* const view,
      mojo::PendingReceiver<viz::mojom::LayeredWindowUpdater> receiver);

  CefLayeredWindowUpdaterOSR(const CefLayeredWindowUpdaterOSR&) = delete;
  CefLayeredWindowUpdaterOSR& operator=(const CefLayeredWindowUpdaterOSR&) =
      delete;

  ~CefLayeredWindowUpdaterOSR() override;

  void SetActive(bool active);
  const void* GetPixelMemory() const;
  gfx::Size GetPixelSize() const;

  // viz::mojom::LayeredWindowUpdater implementation.
  void OnAllocatedSharedMemory(const gfx::Size& pixel_size,
                               base::UnsafeSharedMemoryRegion region) override;
  void Draw(const gfx::Rect& damage_rect, DrawCallback draw_callback) override;

 private:
  CefRenderWidgetHostViewOSR* const view_;
  mojo::Receiver<viz::mojom::LayeredWindowUpdater> receiver_;
  bool active_ = false;
  base::WritableSharedMemoryMapping shared_memory_;
  gfx::Size pixel_size_;
};

CefLayeredWindowUpdaterOSR::CefLayeredWindowUpdaterOSR(
    CefRenderWidgetHostViewOSR* const view,
    mojo::PendingReceiver<viz::mojom::LayeredWindowUpdater> receiver)
    : view_(view), receiver_(this, std::move(receiver)) {}

CefLayeredWindowUpdaterOSR::~CefLayeredWindowUpdaterOSR() = default;

void CefLayeredWindowUpdaterOSR::SetActive(bool active) {
  active_ = active;
}

const void* CefLayeredWindowUpdaterOSR::GetPixelMemory() const {
  return shared_memory_.memory();
}

gfx::Size CefLayeredWindowUpdaterOSR::GetPixelSize() const {
  return pixel_size_;
}

void CefLayeredWindowUpdaterOSR::OnAllocatedSharedMemory(
    const gfx::Size& pixel_size,
    base::UnsafeSharedMemoryRegion region) {
  // Make sure |pixel_size| is sane.
  size_t expected_bytes;
  if (!viz::ResourceSizes::MaybeSizeInBytes(
          pixel_size, viz::SinglePlaneFormat::kRGBA_8888, &expected_bytes)) {
    DLOG(ERROR) << "OnAllocatedSharedMemory with size that overflows";
    return;
  }

  auto mapping = region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  if (mapping.size() < expected_bytes) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  pixel_size_ = pixel_size;
  shared_memory_ = std::move(mapping);
}

void CefLayeredWindowUpdaterOSR::Draw(const gfx::Rect& damage_rect,
                                      DrawCallback draw_callback) {
  if (active_) {
    const void* memory = GetPixelMemory();
    if (memory) {
      view_->OnPaint(damage_rect, pixel_size_, memory);
    } else {
      LOG(WARNING) << "Failed to read pixels";
    }
  }

  std::move(draw_callback).Run();
}

CefHostDisplayClientOSR::CefHostDisplayClientOSR(
    CefRenderWidgetHostViewOSR* const view,
    gfx::AcceleratedWidget widget)
    : viz::HostDisplayClient(widget), view_(view) {}

CefHostDisplayClientOSR::~CefHostDisplayClientOSR() {}

void CefHostDisplayClientOSR::SetActive(bool active) {
  active_ = active;
  if (layered_window_updater_) {
    layered_window_updater_->SetActive(active_);
  }
}

const void* CefHostDisplayClientOSR::GetPixelMemory() const {
  return layered_window_updater_ ? layered_window_updater_->GetPixelMemory()
                                 : nullptr;
}

gfx::Size CefHostDisplayClientOSR::GetPixelSize() const {
  return layered_window_updater_ ? layered_window_updater_->GetPixelSize()
                                 : gfx::Size{};
}

void CefHostDisplayClientOSR::UseProxyOutputDevice(
    UseProxyOutputDeviceCallback callback) {
  std::move(callback).Run(true);
}

void CefHostDisplayClientOSR::CreateLayeredWindowUpdater(
    mojo::PendingReceiver<viz::mojom::LayeredWindowUpdater> receiver) {
  layered_window_updater_ =
      std::make_unique<CefLayeredWindowUpdaterOSR>(view_, std::move(receiver));
  layered_window_updater_->SetActive(active_);
}

#if BUILDFLAG(IS_LINUX)
void CefHostDisplayClientOSR::DidCompleteSwapWithNewSize(
    const gfx::Size& size) {}
#endif
