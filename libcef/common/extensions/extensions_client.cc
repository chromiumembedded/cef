// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/extensions_client.h"

#include <utility>

#include "libcef/common/cef_switches.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "cef/libcef/common/extensions/api/generated_schemas.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/api_feature.h"
#include "extensions/common/features/base_feature_provider.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/json_feature_provider_source.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/permission_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/permissions_provider.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/cef_resources.h"
#include "grit/extensions_resources.h"

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

// TODO(jamescook): Refactor ChromePermissionsMessageProvider so we can share
// code. For now, this implementation does nothing.
class CefPermissionMessageProvider : public PermissionMessageProvider {
 public:
  CefPermissionMessageProvider() {}
  ~CefPermissionMessageProvider() override {}

  // PermissionMessageProvider implementation.
  PermissionMessages GetPermissionMessages(
      const PermissionIDSet& permissions) const override {
    return PermissionMessages();
  }

  bool IsPrivilegeIncrease(const PermissionSet& old_permissions,
                           const PermissionSet& new_permissions,
                           Manifest::Type extension_type) const override {
    // Ensure we implement this before shipping.
    CHECK(false);
    return false;
  }

  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override {
    return PermissionIDSet();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CefPermissionMessageProvider);
};

base::LazyInstance<CefPermissionMessageProvider>
    g_permission_message_provider = LAZY_INSTANCE_INITIALIZER;

}  // namespace

CefExtensionsClient::CefExtensionsClient()
    : extensions_api_permissions_(ExtensionsAPIPermissions()) {
}

CefExtensionsClient::~CefExtensionsClient() {
}

void CefExtensionsClient::Initialize() {
  RegisterCommonManifestHandlers();
  ManifestHandler::FinalizeRegistration();
  // TODO(jamescook): Do we need to whitelist any extensions?

  PermissionsInfo::GetInstance()->AddProvider(extensions_api_permissions_);
}

const PermissionMessageProvider&
CefExtensionsClient::GetPermissionMessageProvider() const {
  NOTIMPLEMENTED();
  return g_permission_message_provider.Get();
}

const std::string CefExtensionsClient::GetProductName() {
  return "cef";
}

scoped_ptr<FeatureProvider> CefExtensionsClient::CreateFeatureProvider(
    const std::string& name) const {
  scoped_ptr<FeatureProvider> provider;
  scoped_ptr<JSONFeatureProviderSource> source(
      CreateFeatureProviderSource(name));
  if (name == "api") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<APIFeature>));
  } else if (name == "manifest") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<ManifestFeature>));
  } else if (name == "permission") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<PermissionFeature>));
  } else if (name == "behavior") {
    provider.reset(new BaseFeatureProvider(source->dictionary(),
                                           CreateFeature<BehaviorFeature>));
  } else {
    NOTREACHED();
  }
  return provider;
}

scoped_ptr<JSONFeatureProviderSource>
CefExtensionsClient::CreateFeatureProviderSource(
    const std::string& name) const {
  scoped_ptr<JSONFeatureProviderSource> source(
      new JSONFeatureProviderSource(name));
  if (name == "api") {
    source->LoadJSON(IDR_EXTENSION_API_FEATURES);

    // Extension API features specific to CEF. See
    // libcef/common/extensions/api/README.txt for additional details. 
    source->LoadJSON(IDR_CEF_EXTENSION_API_FEATURES);
  } else if (name == "manifest") {
    source->LoadJSON(IDR_EXTENSION_MANIFEST_FEATURES);
  } else if (name == "permission") {
    source->LoadJSON(IDR_EXTENSION_PERMISSION_FEATURES);
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
  return api::GeneratedSchemas::IsGenerated(name) ||
         api::ChromeGeneratedSchemas::IsGenerated(name);
}

base::StringPiece CefExtensionsClient::GetAPISchema(
    const std::string& name) const {
  // Schema for CEF-only APIs.
  if (api::ChromeGeneratedSchemas::IsGenerated(name))
    return api::ChromeGeneratedSchemas::Get(name);

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
