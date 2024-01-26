// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/extensions_api_provider.h"

#include "libcef/common/extensions/chrome_generated_schemas.h"

#include "cef/grit/cef_resources.h"
// #include "cef/libcef/common/extensions/api/generated_schemas.h"
#include "cef/libcef/common/extensions/api/cef_api_features.h"
#include "cef/libcef/common/extensions/api/cef_manifest_features.h"
#include "cef/libcef/common/extensions/api/cef_permission_features.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {

CefExtensionsAPIProvider::CefExtensionsAPIProvider() = default;

void CefExtensionsAPIProvider::AddAPIFeatures(FeatureProvider* provider) {
  AddCEFAPIFeatures(provider);
}

void CefExtensionsAPIProvider::AddManifestFeatures(FeatureProvider* provider) {
  AddCEFManifestFeatures(provider);
}

void CefExtensionsAPIProvider::AddPermissionFeatures(
    FeatureProvider* provider) {
  AddCEFPermissionFeatures(provider);
}

void CefExtensionsAPIProvider::AddBehaviorFeatures(FeatureProvider* provider) {
  // No CEF-specific behavior features.
}

void CefExtensionsAPIProvider::AddAPIJSONSources(
    JSONFeatureProviderSource* json_source) {
  // Extension API features specific to CEF. See
  // libcef/common/extensions/api/README.txt for additional details.
  json_source->LoadJSON(IDR_CEF_EXTENSION_API_FEATURES);
}

bool CefExtensionsAPIProvider::IsAPISchemaGenerated(const std::string& name) {
  // Schema for CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  // if (api::cef::CefGeneratedSchemas::IsGenerated(name))
  //   return true;

  // Chrome APIs whitelisted by CEF.
  if (api::cef::ChromeGeneratedSchemas::IsGenerated(name)) {
    return true;
  }

  return false;
}

std::string_view CefExtensionsAPIProvider::GetAPISchema(
    const std::string& name) {
  // Schema for CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  // if (api::cef::CefGeneratedSchemas::IsGenerated(name))
  //   return api::cef::CefGeneratedSchemas::Get(name);

  // Chrome APIs whitelisted by CEF.
  if (api::cef::ChromeGeneratedSchemas::IsGenerated(name)) {
    return api::cef::ChromeGeneratedSchemas::Get(name);
  }

  return std::string_view();
}

void CefExtensionsAPIProvider::RegisterPermissions(
    PermissionsInfo* permissions_info) {
  permissions_info->RegisterPermissions(
      chrome_api_permissions::GetPermissionInfos(),
      chrome_api_permissions::GetPermissionAliases());
}

void CefExtensionsAPIProvider::RegisterManifestHandlers() {
  RegisterChromeManifestHandlers();
}

}  // namespace extensions
