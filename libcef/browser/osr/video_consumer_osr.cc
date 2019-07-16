// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "libcef/browser/osr/video_consumer_osr.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/osr/render_widget_host_view_osr.h"

#include "media/base/video_frame_metadata.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "ui/gfx/skbitmap_operations.h"

CefVideoConsumerOSR::CefVideoConsumerOSR(CefRenderWidgetHostViewOSR* view)
    : view_(view),
      video_capturer_(view->CreateVideoCapturer()),
      weak_ptr_factory_(this) {
  const gfx::Size view_size = view_->SizeInPixels();
  video_capturer_->SetResolutionConstraints(view_size, view_size, true);
  video_capturer_->SetAutoThrottlingEnabled(false);
  video_capturer_->SetMinSizeChangePeriod(base::TimeDelta());
  video_capturer_->SetFormat(media::PIXEL_FORMAT_ARGB,
                             gfx::ColorSpace::CreateREC709());
}

CefVideoConsumerOSR::~CefVideoConsumerOSR() = default;

void CefVideoConsumerOSR::SetActive(bool active) {
  if (active) {
    video_capturer_->Start(this);
  } else {
    video_capturer_->Stop();
  }
}

void CefVideoConsumerOSR::SetFrameRate(base::TimeDelta frame_rate) {
  video_capturer_->SetMinCapturePeriod(frame_rate);
}

void CefVideoConsumerOSR::SizeChanged() {
  const gfx::Size view_size = view_->SizeInPixels();
  video_capturer_->SetResolutionConstraints(view_size, view_size, true);
  video_capturer_->RequestRefreshFrame();
}

void CefVideoConsumerOSR::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    ::media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) {
  const gfx::Size view_size = view_->SizeInPixels();
  if (view_size != content_rect.size()) {
    video_capturer_->SetResolutionConstraints(view_size, view_size, true);
    video_capturer_->RequestRefreshFrame();
    return;
  }

  if (!data.IsValid()) {
    callbacks->Done();
    return;
  }
  base::ReadOnlySharedMemoryMapping mapping = data.Map();
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

  media::VideoFrameMetadata metadata;
  metadata.MergeInternalValuesFrom(info->metadata);
  gfx::Rect damage_rect;

  if (!metadata.GetRect(media::VideoFrameMetadata::CAPTURE_UPDATE_RECT,
                        &damage_rect)) {
    damage_rect = content_rect;
  }

  view_->OnPaint(damage_rect, content_rect.size(), pixels);
}

void CefVideoConsumerOSR::OnStopped() {}
