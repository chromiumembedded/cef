#ifndef LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_
#define LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_

#include "base/callback.h"
#include "base/optional.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

class CefRenderWidgetHostViewOSR;

class CefVideoConsumerOSR : public viz::mojom::FrameSinkVideoConsumer {
 public:
  explicit CefVideoConsumerOSR(CefRenderWidgetHostViewOSR* view);
  ~CefVideoConsumerOSR() override;

  void SetActive(bool active);
  void SetFrameRate(base::TimeDelta frame_rate);
  void SizeChanged(const gfx::Size& size_in_pixels);
  void RequestRefreshFrame(const base::Optional<gfx::Rect>& bounds_in_pixels);

 private:
  // viz::mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnStopped() override;

  CefRenderWidgetHostViewOSR* const view_;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  gfx::Size size_in_pixels_;
  base::Optional<gfx::Rect> bounds_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(CefVideoConsumerOSR);
};

#endif  // LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_