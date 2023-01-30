#ifndef CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_PROXY_H_
#define CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_PROXY_H_

#include "base/memory/shared_memory_mapping.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/compositing/layered_window_updater.mojom.h"

namespace viz {

// SoftwareOutputDevice implementation that draws indirectly. An
// implementation of mojom::LayeredWindowUpdater in the browser process
// handles the actual drawing. Pixel backing is in SharedMemory so no copying
// between processes is required.
class VIZ_SERVICE_EXPORT SoftwareOutputDeviceProxy
    : public SoftwareOutputDevice {
 public:
  explicit SoftwareOutputDeviceProxy(
      mojo::PendingRemote<mojom::LayeredWindowUpdater> layered_window_updater);

  SoftwareOutputDeviceProxy(const SoftwareOutputDeviceProxy&) = delete;
  SoftwareOutputDeviceProxy& operator=(const SoftwareOutputDeviceProxy&) =
      delete;

  ~SoftwareOutputDeviceProxy() override;

  // SoftwareOutputDevice implementation.
  void OnSwapBuffers(SwapBuffersCallback swap_ack_callback,
                     gfx::FrameData data) override;

  // SoftwareOutputDeviceBase implementation.
  void Resize(const gfx::Size& viewport_pixel_size,
              float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;

 private:
  // Runs |swap_ack_callback_| after draw has happened.
  void DrawAck();

  mojo::Remote<mojom::LayeredWindowUpdater> layered_window_updater_;

  std::unique_ptr<SkCanvas> canvas_;
  bool waiting_on_draw_ack_ = false;
  bool in_paint_ = false;
  base::OnceClosure swap_ack_callback_;
  base::WritableSharedMemoryMapping shm_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace viz

#endif  // CEF_LIBCEF_BROWSER_OSR_SOFTWARE_OUTPUT_DEVICE_PROXY_H_