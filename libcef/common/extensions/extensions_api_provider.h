// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_API_PROVIDER_H_
#define CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_API_PROVIDER_H_

#include "extensions/common/extensions_api_provider.h"

namespace extensions {

class CefExtensionsAPIProvider : public ExtensionsAPIProvider {
 public:
  CefExtensionsAPIProvider();

  CefExtensionsAPIProvider(const CefExtensionsAPIProvider&) = delete;
  CefExtensionsAPIProvider& operator=(const CefExtensionsAPIProvider&) = delete;

  // ExtensionsAPIProvider:
  void AddAPIFeatures(FeatureProvider* provider) override;
  void AddManifestFeatures(FeatureProvider* provider) override;
  void AddPermissionFeatures(FeatureProvider* provider) override;
  void AddBehaviorFeatures(FeatureProvider* provider) override;
  void AddAPIJSONSources(JSONFeatureProviderSource* json_source) override;
  bool IsAPISchemaGenerated(const std::string& name) override;
  std::string_view GetAPISchema(const std::string& name) override;
  void RegisterPermissions(PermissionsInfo* permissions_info) override;
  void RegisterManifestHandlers() override;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_API_PROVIDER_H_
