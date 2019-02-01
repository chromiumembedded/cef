// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_packaged_service_manifests.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/constants.mojom.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/startup_metric_utils/common/startup_metric.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "services/proxy_resolver/proxy_resolver_manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_MACOSX)
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#endif

#include "chrome/services/printing/manifest.h"
#include "components/services/pdf_compositor/pdf_compositor_manifest.h"  // nogncheck

namespace {

const service_manager::Manifest& GetCefManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(chrome::mojom::kServiceName)
        .WithDisplayName("CEF")
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSharedAcrossGroups)
                         .CanConnectToInstancesWithAnyId(true)
                         .CanRegisterOtherServiceInstances(true)
                         .Build())
        .ExposeCapability("renderer",
                          service_manager::Manifest::InterfaceList<
#if defined(OS_MACOSX)
                              spellcheck::mojom::SpellCheckPanelHost,
#endif
                              spellcheck::mojom::SpellCheckHost,
                              startup_metric_utils::mojom::StartupMetricHost>())
        .RequireCapability(chrome::mojom::kRendererServiceName, "browser")
        .Build()
  };
  return *manifest;
}

}  // namespace

const std::vector<service_manager::Manifest>& GetCefPackagedServiceManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests{{
      GetCefManifest(),
      proxy_resolver::GetManifest(),
      pdf_compositor::GetManifest(),
      chrome_printing::GetManifest(),
  }};
  return *manifests;
}
