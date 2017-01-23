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
#include "cef/libcef/common/extensions/api/cef_api_features.h"
#include "cef/libcef/common/extensions/api/cef_behavior_features.h"
#include "cef/libcef/common/extensions/api/cef_manifest_features.h"
#include "cef/libcef/common/extensions/api/cef_permission_features.h"
#include "chrome/common/extensions/chrome_aliases.h"
#include "chrome/common/extensions/chrome_manifest_handlers.h"
#include "chrome/grit/common_resources.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extensions_aliases.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/json_feature_provider_source.h"
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

CefExtensionsClient::CefExtensionsClient()
  : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
    webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
}

CefExtensionsClient::~CefExtensionsClient() {
}

void CefExtensionsClient::Initialize() {
  RegisterCommonManifestHandlers();
  RegisterChromeManifestHandlers();
  ManifestHandler::FinalizeRegistration();
  // TODO(jamescook): Do we need to whitelist any extensions?

  // Set up permissions.
  PermissionsInfo::GetInstance()->AddProvider(chrome_api_permissions_,
                                              GetChromePermissionAliases());
  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_,
                                              GetExtensionsPermissionAliases());
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
  if (name == "api") {
    provider.reset(new CefAPIFeatureProvider());
  } else if (name == "manifest") {
    provider.reset(new CefManifestFeatureProvider());
  } else if (name == "permission") {
    provider.reset(new CefPermissionFeatureProvider());
  } else if (name == "behavior") {
    provider.reset(new CefBehaviorFeatureProvider());
  } else {
    NOTREACHED();
  }
  return provider;
}

std::unique_ptr<JSONFeatureProviderSource>
CefExtensionsClient::CreateAPIFeatureSource() const {
  std::unique_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource("api"));
  source->LoadJSON(IDR_EXTENSION_API_FEATURES);

  // Extension API features specific to CEF. See
  // libcef/common/extensions/api/README.txt for additional details.
  source->LoadJSON(IDR_CEF_EXTENSION_API_FEATURES);

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

bool CefExtensionsClient::ShouldSuppressFatalErrors() const {
  return true;
}

void CefExtensionsClient::RecordDidSuppressFatalError() {
}

const GURL& CefExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& CefExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool CefExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  // TODO(rockot): Maybe we want to do something else here. For now we accept
  // any URL as a blacklist URL because we don't really care.
  return true;
}

}  // namespace extensions
