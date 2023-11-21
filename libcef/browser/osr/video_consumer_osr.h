#ifndef LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_
#define LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_

#include "base/functional/callback.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class CefRenderWidgetHostViewOSR;

class CefVideoConsumerOSR : public viz::mojom::FrameSinkVideoConsumer {
 public:
  explicit CefVideoConsumerOSR(CefRenderWidgetHostViewOSR* view);

  CefVideoConsumerOSR(const CefVideoConsumerOSR&) = delete;
  CefVideoConsumerOSR& operator=(const CefVideoConsumerOSR&) = delete;

  ~CefVideoConsumerOSR() override;

  void SetActive(bool active);
  void SetFrameRate(base::TimeDelta frame_rate);
  void SizeChanged(const gfx::Size& size_in_pixels);
  void RequestRefreshFrame(const absl::optional<gfx::Rect>& bounds_in_pixels);

 private:
  // viz::mojom::FrameSinkVideoConsumer implementation.
  void OnFrameCaptured(
      media::mojom::VideoBufferHandlePtr data,
      media::mojom::VideoFrameInfoPtr info,
      const gfx::Rect& content_rect,
      mojo::PendingRemote<viz::mojom::FrameSinkVideoConsumerFrameCallbacks>
          callbacks) override;
  void OnFrameWithEmptyRegionCapture() override {}
  void OnStopped() override {}
  void OnLog(const std::string& message) override {}
  void OnNewSubCaptureTargetVersion(
      uint32_t sub_capture_target_version) override {}

  CefRenderWidgetHostViewOSR* const view_;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> video_capturer_;

  gfx::Size size_in_pixels_;
  absl::optional<gfx::Rect> bounds_in_pixels_;
};

#endif  // LIBCEF_BROWSER_OSR_VIDEO_CONSUMER_OSR_H_
