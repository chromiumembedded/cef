// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_CONTENT_UTILITY_OVERLAY_MANIFEST_H_
#define CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_CONTENT_UTILITY_OVERLAY_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

// Returns the Manifest CEF amends to Contents's content_utility service
// manifest. This allows CEF to extend the capabilities exposed and/or
// required by content_utility service instances
const service_manager::Manifest& GetCefContentUtilityOverlayManifest();

#endif  // CEF_LIBCEF_COMMON_SERVICE_MANIFESTS_CEF_CONTENT_UTILITY_OVERLAY_MANIFEST_H_
