// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/extensions/extensions_client.h"

#include <utility>

#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_api_provider.h"

#include "base/logging.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace {

template <class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass;
}

}  // namespace

CefExtensionsClient::CefExtensionsClient()
    : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
      new_webstore_base_url_(extension_urls::kNewChromeWebstoreBaseURL),
      webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<CefExtensionsAPIProvider>());
}

CefExtensionsClient::~CefExtensionsClient() = default;

void CefExtensionsClient::Initialize() {}

void CefExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {}

const PermissionMessageProvider&
CefExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string CefExtensionsClient::GetProductName() {
  return "cef";
}

void CefExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  NOTIMPLEMENTED();
}

void CefExtensionsClient::SetScriptingAllowlist(
    const ScriptingAllowlist& allowlist) {
  scripting_allowlist_ = allowlist;
}

const ExtensionsClient::ScriptingAllowlist&
CefExtensionsClient::GetScriptingAllowlist() const {
  // TODO(jamescook): Real allowlist.
  return scripting_allowlist_;
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

const GURL& CefExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& CefExtensionsClient::GetNewWebstoreBaseURL() const {
  return new_webstore_base_url_;
}

const GURL& CefExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool CefExtensionsClient::IsBlocklistUpdateURL(const GURL& url) const {
  // TODO(rockot): Maybe we want to do something else here. For now we accept
  // any URL as a blocklist URL because we don't really care.
  return true;
}

}  // namespace extensions
