// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_RENDERER_MANIFEST_H_
#define CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_RENDERER_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

// Returns the Manifest for the cef_renderer service. CEF registers an
// instance of this service for each renderer process started by Content, and
// that instance lives in the corresponding renderer process alongside the
// content_renderer instance.
const service_manager::Manifest& GetCefRendererManifest();

#endif  // CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_RENDERER_MANIFEST_H_
