// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"

#include "base/feature_list.h"
#include "chrome/grit/renderer_resources.h"
#include "extensions/common/extension_features.h"
#include "extensions/renderer/resource_bundle_source_map.h"

namespace extensions {

CefExtensionsDispatcherDelegate::CefExtensionsDispatcherDelegate() = default;

CefExtensionsDispatcherDelegate::~CefExtensionsDispatcherDelegate() = default;

void CefExtensionsDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {}

}  // namespace extensions
