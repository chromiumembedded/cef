// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/extensions/browser_context_keyed_service_factories.h"

#include "libcef/browser/extensions/api/streams_private/streams_private_api.h"

#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {
namespace cef {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  RendererStartupHelperFactory::GetInstance();
  StreamsPrivateAPI::GetFactoryInstance();
}

}  // namespace cef
}  // namespace extensions
