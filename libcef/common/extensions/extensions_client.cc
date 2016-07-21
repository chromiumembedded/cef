// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/extensions_client.h"

#include <utility>

#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/chrome_generated_schemas.h"

#include "base/logging.h"
#include "cef/grit/cef_resources.h"
//#include "cef/libcef/common/extensions/api/generated_schemas.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/json_feature_provider.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/permissions_provider.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/grit/extensions_resources.h"

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

}  // namespace

CefExtensionsClient::CefExtensionsClient() {
}

CefExtensionsClient::~CefExtensionsClient() {
}

void CefExtensionsClient::Initialize() {
  RegisterCommonManifestHandlers();
  RegisterChromeManifestHandlers();
  ManifestHandler::FinalizeRegistration();
  // TODO(jamescook): Do we need to whitelist any extensions?

  // Set up permissions.
  PermissionsInfo::GetInstance()->AddProvider(chrome_api_permissions_);
  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_);
}

const PermissionMessageProvider&
CefExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string CefExtensionsClient::GetProductName() {
  return "cef";
}

std::unique_ptr<FeatureProvider> CefExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  std::unique_ptr<FeatureProvider> provider;
  std::unique_ptr<JSONFeatureProviderSource> source(
      CreateFeatureProviderSource(name));
  if (name == "api") {
    provider.reset(new JSONFeatureProvider(source->dictionary(),
                                           CreateFeature<APIFeature>));
  } else if (name == "manifest") {
    provider.reset(new JSONFeatureProvider(source->dictionary(),
                                           CreateFeature<ManifestFeature>));
  } else if (name == "permission") {
    provider.reset(new JSONFeatureProvider(source->dictionary(),
                                           CreateFeature<PermissionFeature>));
  } else if (name == "behavior") {
    provider.reset(new JSONFeatureProvider(source->dictionary(),
                                           CreateFeature<BehaviorFeature>));
  } else {
    NOTREACHED();
  }
  return provider;
}

std::unique_ptr<JSONFeatureProviderSource>
CefExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  std::unique_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource(name));
  if (name == "api") {
    source->LoadJSON(IDR_EXTENSION_API_FEATURES);

    // Extension API features specific to CEF. See
    // libcef/common/extensions/api/README.txt for additional details.
    source->LoadJSON(IDR_CEF_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_EXTENSION_MANIFEST_FEATURES);

    // Use the same manifest features as Chrome.
    source->LoadJSON(IDR_CHROME_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_EXTENSION_PERMISSION_FEATURES);

    // Extension permission features specific to CEF. See
    // libcef/common/extensions/api/README.txt for additional details.
    source->LoadJSON(IDR_CEF_EXTENSION_PERMISSION_FEATURES);
  } else if (name == "behavior") {
    source->LoadJSON(IDR_EXTENSION_BEHAVIOR_FEATURES);
  } else {
    NOTREACHED();
    source.reset();
  }
  return source;
}

void CefExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  NOTIMPLEMENTED();
}

void CefExtensionsClient::SetScriptingWhitelist(
    const ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const ExtensionsClient::ScriptingWhitelist&
CefExtensionsClient::GetScriptingWhitelist() const {
  // TODO(jamescook): Real whitelist.
  return scripting_whitelist_;
}

URLPatternSet CefExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  return URLPatternSet();
}

bool CefExtensionsClient::IsScriptableURL(const GURL& url,
                                          std::string* error) const {
  return true;
}

bool CefExtensionsClient::IsAPISchemaGenerated(
    const std::string& name) const {
  // Schema for CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  //if (api::cef::CefGeneratedSchemas::IsGenerated(name))
  //  return true;

  // Chrome APIs whitelisted by CEF.
  if (api::cef::ChromeGeneratedSchemas::IsGenerated(name))
    return true;

  // Core extensions APIs.
  if (api::GeneratedSchemas::IsGenerated(name))
    return true;

  return false;
}

base::StringPiece CefExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // Schema for CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  //if (api::cef::CefGeneratedSchemas::IsGenerated(name))
  //  return api::cef::CefGeneratedSchemas::Get(name);

  // Chrome APIs whitelisted by CEF.
  if (api::cef::ChromeGeneratedSchemas::IsGenerated(name))
    return api::cef::ChromeGeneratedSchemas::Get(name);

  // Core extensions APIs.
  return api::GeneratedSchemas::Get(name);
}

void CefExtensionsClient::RegisterAPISchemaResources(
    ExtensionAPI* api) const {
}

bool CefExtensionsClient::ShouldSuppressFatalErrors() const {
  return true;
}

void CefExtensionsClient::RecordDidSuppressFatalError() {
}

std::string CefExtensionsClient::GetWebstoreBaseURL() const {
  return extension_urls::kChromeWebstoreBaseURL;
}

std::string CefExtensionsClient::GetWebstoreUpdateURL() const {
  return extension_urls::kChromeWebstoreUpdateURL;
}

bool CefExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  // TODO(rockot): Maybe we want to do something else here. For now we accept
  // any URL as a blacklist URL because we don't really care.
  return true;
}

}  // namespace extensions
