// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_content_utility_overlay_manifest.h"

#include "base/no_destructor.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetCefContentUtilityOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability("browser",
                            service_manager::Manifest::InterfaceList<
                                heap_profiling::mojom::ProfilingClient>())
          .Build()};
  return *manifest;
}
