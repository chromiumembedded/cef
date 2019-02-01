// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_content_renderer_overlay_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "components/subresource_filter/content/mojom/subresource_filter_agent.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_MACOSX)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif

const service_manager::Manifest& GetCefContentRendererOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .ExposeCapability("browser",
                          service_manager::Manifest::InterfaceList<
                              heap_profiling::mojom::ProfilingClient>())
#if defined(OS_MACOSX)
        .ExposeInterfaceFilterCapability_Deprecated(
            "navigation:frame", "browser",
            service_manager::Manifest::InterfaceList<
                spellcheck::mojom::SpellCheckPanel>())
#endif
        .Build()
  };
  return *manifest;
}
