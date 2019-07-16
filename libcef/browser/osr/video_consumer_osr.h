#ifndef LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_
#define LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"

class CefRenderWidgetHostViewOSR;

class CefVideoConsumerOSR : public viz::mojom::FrameSinkVideoConsumer {
 public:
  explicit CefVideoConsumerOSR(CefRenderWidgetHostViewOSR* view);
  ~CefVideoConsumerOSR() override;

  void SetActive(bool active);
  void SetFrameRate(base::TimeDelta frame_rate);
  void SizeChanged();

 private:
  // viz::mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      base::ReadOnlySharedMemoryRegion data,
      ::media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      viz::mojom::FrameSinkVideoConsumerFrameCallbacksPtr callbacks) override;
  void OnStopped() override;

  CefRenderWidgetHostViewOSR* const view_;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  base::WeakPtrFactory<CefVideoConsumerOSR> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefVideoConsumerOSR);
};

#endif  // LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_