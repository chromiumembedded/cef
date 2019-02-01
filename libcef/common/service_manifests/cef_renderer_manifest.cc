// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/service_manifests/cef_renderer_manifest.h"

#include "base/no_destructor.h"
#include "chrome/common/constants.mojom.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetCefRendererManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kRendererServiceName)
          .ExposeCapability("browser", service_manager::Manifest::InterfaceList<
                                           spellcheck::mojom::SpellChecker>())
          .RequireCapability(chrome::mojom::kServiceName, "renderer")
          .Build()};
  return *manifest;
}
