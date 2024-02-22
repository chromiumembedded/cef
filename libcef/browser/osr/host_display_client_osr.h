// Copyright 2019 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/shared_memory_mapping.h"
#include "components/viz/host/host_display_client.h"
#include "ui/gfx/native_widget_types.h"

class CefLayeredWindowUpdaterOSR;
class CefRenderWidgetHostViewOSR;

class CefHostDisplayClientOSR : public viz::HostDisplayClient {
 public:
  CefHostDisplayClientOSR(CefRenderWidgetHostViewOSR* const view,
                          gfx::AcceleratedWidget widget);

  CefHostDisplayClientOSR(const CefHostDisplayClientOSR&) = delete;
  CefHostDisplayClientOSR& operator=(const CefHostDisplayClientOSR&) = delete;

  ~CefHostDisplayClientOSR() override;

  void SetActive(bool active);
  const void* GetPixelMemory() const;
  gfx::Size GetPixelSize() const;

 private:
  // mojom::DisplayClient implementation.
  void UseProxyOutputDevice(UseProxyOutputDeviceCallback callback) override;

  void CreateLayeredWindowUpdater(
      mojo::PendingReceiver<viz::mojom::LayeredWindowUpdater> receiver)
      override;

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
  void DidCompleteSwapWithNewSize(const gfx::Size& size) override;
#endif

  CefRenderWidgetHostViewOSR* const view_;
  std::unique_ptr<CefLayeredWindowUpdaterOSR> layered_window_updater_;
  bool active_ = false;
};

#endif  // CEF_LIBCEF_BROWSER_OSR_HOST_DISPLAY_CLIENT_OSR_H_
