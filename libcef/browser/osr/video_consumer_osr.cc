// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/osr/video_consumer_osr.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "cef/include/cef_callback.h"
#include "cef/libcef/browser/osr/render_widget_host_view_osr.h"
#include "cef/libcef/browser/thread_util.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "ui/gfx/skbitmap_operations.h"

namespace {

// The capturer redelivers unchanged frames without taking a buffer from its
// pool, so the pool does not bound how many frames a client holds.
constexpr int kMaxHeldFrames = 4;

// Holds a shared texture frame and its handles until executed or destroyed.
// Releases on the UI thread, where |on_released_| reaches the consumer, and in
// a later task when executed so that the frame outlives the paint call. Closing
// |callbacks_| releases the frame like Done() (see viz::InFlightFrameDelivery).
class CefAcceleratedPaintCallbackImpl : public CefCallback {
 public:
  CefAcceleratedPaintCallbackImpl(
      media::mojom::VideoBufferHandlePtr data,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks,
      base::OnceClosure on_released)
      : on_released_(std::move(on_released)),
        data_(std::move(data)),
        callbacks_(std::move(callbacks)) {}

  CefAcceleratedPaintCallbackImpl(const CefAcceleratedPaintCallbackImpl&) =
      delete;
  CefAcceleratedPaintCallbackImpl& operator=(
      const CefAcceleratedPaintCallbackImpl&) = delete;

  void Continue() override {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefAcceleratedPaintCallbackImpl::ReleaseFrame, this));
  }

  void Cancel() override { Continue(); }

 private:
  void ReleaseFrame() {
    data_.reset();
    callbacks_.reset();
    on_released_.RunAndReset();
  }

  base::ScopedClosureRunner on_released_;
  media::mojom::VideoBufferHandlePtr data_;
  mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
      callbacks_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefAcceleratedPaintCallbackImpl);
};

}  // namespace

CefVideoConsumerOSR::CefVideoConsumerOSR(CefRenderWidgetHostViewOSR* view,
                                         bool use_shared_texture)
    : use_shared_texture_(use_shared_texture),
      view_(view),
      video_capturer_(view->CreateVideoCapturer()) {
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB);
  video_capturer_->SetAnimationFpsLockIn(false, 0.0);

  // Always use the highest resolution within constraints that doesn't exceed
  // the source size.
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());

  SizeChanged(view_->SizeInPixels());
  SetActive(true);
}

CefVideoConsumerOSR::~CefVideoConsumerOSR() = default;

void CefVideoConsumerOSR::SetActive(bool active) {
  if (active) {
    video_capturer_->Start(this,
                           use_shared_texture_
#if BUILDFLAG(IS_LINUX)
                               ? viz::mojom::BufferFormatPreference::
                                     kPreferSharedImageWithNativeHandle
#else
                               ? viz::mojom::BufferFormatPreference::
                                     kPreferMappableSharedImage
#endif
                               : viz::mojom::BufferFormatPreference::kDefault);
  } else {
    video_capturer_->Stop();
  }
}

void CefVideoConsumerOSR::SetFrameRate(base::TimeDelta frame_rate) {
  video_capturer_->SetMinCapturePeriod(frame_rate);
}

void CefVideoConsumerOSR::SizeChanged(const gfx::Size& size_in_pixels) {
  if (size_in_pixels_ == size_in_pixels) {
    return;
  }
  size_in_pixels_ = size_in_pixels;

  // Capture resolution will be held constant.
  video_capturer_->SetResolutionConstraints(size_in_pixels, size_in_pixels,
                                            true /* use_fixed_aspect_ratio */);
}

void CefVideoConsumerOSR::RequestRefreshFrame(
    const std::optional<gfx::Rect>& bounds_in_pixels) {
  bounds_in_pixels_ = bounds_in_pixels;
  video_capturer_->RequestRefreshFrame();
}

void CefVideoConsumerOSR::OnFrameReleased() {
  --held_frames_;
  if (std::exchange(repaint_on_release_, false)) {
    // Frames were dropped with their damage, so repaint the whole view.
    RequestRefreshFrame(gfx::Rect(size_in_pixels_));
  }
}

// Frame size values are as follows:
//   info->coded_size = Width and height of the video frame. Not all pixels in
//   this region are valid.
//   info->visible_rect = Region of coded_size that contains image data, also
//   known as the clean aperture.
//   content_rect = Region of the frame that contains the captured content, with
//   the rest of the frame having been letterboxed to adhere to resolution
//   constraints.
void CefVideoConsumerOSR::OnFrameCaptured(
    media::mojom::VideoBufferHandlePtr data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  // Destroying |callbacks| returns the frame to the capturer's pool.
  media::VideoFrameMetadata metadata = info->metadata;
  gfx::Rect damage_rect;

  if (bounds_in_pixels_) {
    // Use the bounds passed to RequestRefreshFrame().
    damage_rect = gfx::Rect(info->coded_size);
    damage_rect.Intersect(*bounds_in_pixels_);
    bounds_in_pixels_ = std::nullopt;
  } else {
    // Retrieve the rectangular region of the frame that has changed since the
    // frame with the directly preceding CAPTURE_COUNTER. If that frame was not
    // received, typically because it was dropped during transport from the
    // producer, clients must assume that the entire frame has changed.
    // This rectangle is relative to the full frame data, i.e. [0, 0,
    // coded_size.width(), coded_size.height()]. It does not have to be
    // fully contained within visible_rect.
    if (metadata.capture.update_rect) {
      damage_rect = *metadata.capture.update_rect;
    }
    if (damage_rect.IsEmpty()) {
      damage_rect = gfx::Rect(info->coded_size);
    }
  }

  // If it is GPU Texture OSR.
  if (use_shared_texture_) {
    if (held_frames_ == kMaxHeldFrames) {
      repaint_on_release_ = true;
      return;
    }

    CHECK(data->is_gpu_memory_buffer_handle() &&
          (info->pixel_format == media::PIXEL_FORMAT_ARGB ||
           info->pixel_format == media::PIXEL_FORMAT_ABGR));

    // The info->pixel_format will tell if the texture is RGBA or BGRA
    // On Linux, X11 lacks support for RGBA_8888 so it might be BGRA.
    // On Windows and macOS, it should always be RGBA.
    auto pixel_format = info->pixel_format == media::PIXEL_FORMAT_ABGR
                            ? CEF_COLOR_TYPE_RGBA_8888
                            : CEF_COLOR_TYPE_BGRA_8888;

    // Build extra common info.
    cef_accelerated_paint_info_common_t extra = {
        sizeof(cef_accelerated_paint_info_common_t)};
    extra.timestamp = info->timestamp.InMicroseconds();
    extra.coded_size = {info->coded_size.width(), info->coded_size.height()};
    extra.visible_rect = {info->visible_rect.x(), info->visible_rect.y(),
                          info->visible_rect.width(),
                          info->visible_rect.height()};
    extra.content_rect = {content_rect.x(), content_rect.y(),
                          content_rect.width(), content_rect.height()};
    extra.timestamp = info->timestamp.InMicroseconds();

    extra.has_capture_counter = info->metadata.capture.counter.has_value();
    extra.has_capture_update_rect =
        info->metadata.capture.update_rect.has_value();
    extra.has_region_capture_rect =
        info->metadata.capture.region_capture_rect.has_value();
    extra.has_source_size = info->metadata.capture.source_size.has_value();

    extra.capture_counter = info->metadata.capture.counter.value_or(0);
    if (extra.has_capture_update_rect) {
      auto rect = info->metadata.capture.update_rect.value();
      extra.capture_update_rect = {rect.x(), rect.y(), rect.width(),
                                   rect.height()};
    }
    if (extra.has_region_capture_rect) {
      auto rect = info->metadata.capture.region_capture_rect.value();
      extra.region_capture_rect = {rect.x(), rect.y(), rect.width(),
                                   rect.height()};
    }
    if (extra.has_source_size) {
      auto size = info->metadata.capture.source_size.value();
      extra.source_size = {size.width(), size.height()};
    }

#if BUILDFLAG(IS_WIN)
    auto& gmb_handle = data->get_gpu_memory_buffer_handle();
    cef_accelerated_paint_info_t paint_info = {
        sizeof(cef_accelerated_paint_info_t)};
    paint_info.extra = extra;
    paint_info.shared_texture_handle = gmb_handle.dxgi_handle().buffer_handle();
    paint_info.format = pixel_format;
#elif BUILDFLAG(IS_APPLE)
    auto& gmb_handle = data->get_gpu_memory_buffer_handle();
    cef_accelerated_paint_info_t paint_info = {
        sizeof(cef_accelerated_paint_info_t)};
    paint_info.extra = extra;
    paint_info.shared_texture_io_surface = gmb_handle.io_surface().get();
    paint_info.format = pixel_format;
#elif BUILDFLAG(IS_LINUX)
    auto& gmb_handle = data->get_gpu_memory_buffer_handle();
    auto& native_pixmap = gmb_handle.native_pixmap_handle();
    CHECK(native_pixmap.planes.size() <= kAcceleratedPaintMaxPlanes);

    cef_accelerated_paint_info_t paint_info = {
        sizeof(cef_accelerated_paint_info_t)};
    paint_info.extra = extra;
    paint_info.plane_count = native_pixmap.planes.size();
    paint_info.modifier = native_pixmap.modifier;
    paint_info.format = pixel_format;

    auto cef_plain_index = 0;
    for (const auto& plane : native_pixmap.planes) {
      cef_accelerated_paint_native_pixmap_plane_t cef_plane;
      cef_plane.stride = plane.stride;
      cef_plane.offset = plane.offset;
      cef_plane.size = plane.size;
      cef_plane.fd = plane.fd.get();
      paint_info.planes[cef_plain_index++] = cef_plane;
    }
#endif
    ++held_frames_;
    view_->OnAcceleratedPaint(
        damage_rect, info->coded_size, paint_info,
        new CefAcceleratedPaintCallbackImpl(
            std::move(data), std::move(callbacks),
            base::BindOnce(&CefVideoConsumerOSR::OnFrameReleased,
                           weak_ptr_factory_.GetWeakPtr())));
    return;
  }

  // If it is CPU bitmap OSR.
  if (info->pixel_format != media::PIXEL_FORMAT_ARGB) {
    DLOG(ERROR) << "Unsupported pixel format " << info->pixel_format;
    return;
  }

  CHECK(data->is_read_only_shmem_region());
  base::ReadOnlySharedMemoryRegion& shmem_region =
      data->get_read_only_shmem_region();

  // The |data| parameter is not nullable and mojo type mapping for
  // `base::ReadOnlySharedMemoryRegion` defines that nullable version of it is
  // the same type, with null check being equivalent to IsValid() check. Given
  // the above, we should never be able to receive a read only shmem region that
  // is not valid - mojo will enforce it for us.
  DCHECK(shmem_region.IsValid());

  base::ReadOnlySharedMemoryMapping mapping = shmem_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "Shared memory mapping failed.";
    return;
  }
  if (mapping.size() <
      media::VideoFrame::AllocationSize(info->pixel_format, info->coded_size)) {
    DLOG(ERROR) << "Shared memory size was less than expected.";
    return;
  }

  // The SkBitmap's pixels will be marked as immutable, but the installPixels()
  // API requires a non-const pointer. So, cast away the const.
  void* const pixels = const_cast<void*>(mapping.memory());

  view_->OnPaint(damage_rect, info->coded_size, pixels);
}
