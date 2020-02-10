// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_content_browser_overlay_manifest.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#endif

const service_manager::Manifest& GetCefContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("gpu",
                          service_manager::Manifest::InterfaceList<
                              metrics::mojom::CallStackProfileCollector>())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_WIN)
                              mojom::ModuleEventSink,
#endif
                              metrics::mojom::CallStackProfileCollector>())
        .RequireCapability("chrome_printing", "converter")
        .Build()
  };
  return *manifest;
}
