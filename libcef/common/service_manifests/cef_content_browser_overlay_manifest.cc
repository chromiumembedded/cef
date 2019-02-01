// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_content_browser_overlay_manifest.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector.mojom.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_WIN)
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#endif

#include "extensions/common/api/mime_handler.mojom.h"  // nogncheck
#include "extensions/common/mojo/keep_alive.mojom.h"   // nogncheck

const service_manager::Manifest& GetCefContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("gpu",
                          service_manager::Manifest::InterfaceList<
                              metrics::mojom::CallStackProfileCollector>())
        .ExposeCapability("profiling_client",
                          service_manager::Manifest::InterfaceList<
                              heap_profiling::mojom::ProfilingClient>())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_WIN)
                              mojom::ModuleEventSink,
#endif
                              metrics::mojom::CallStackProfileCollector>())
        .RequireCapability("chrome_printing", "converter")
        .RequireCapability("heap_profiling", "heap_profiler")
        .RequireCapability("heap_profiling", "profiling")
        .RequireCapability("pdf_compositor", "compositor")
        .RequireCapability("proxy_resolver", "factory")
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "renderer",
            service_manager::Manifest::InterfaceList<
                extensions::KeepAlive,
                extensions::mime_handler::BeforeUnloadControl,
                extensions::mime_handler::MimeHandlerService>())
        .Build()
  };
  return *manifest;
}
