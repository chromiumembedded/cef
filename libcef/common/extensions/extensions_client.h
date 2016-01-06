// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_
#define CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/extensions_api_permissions.h"

namespace extensions {

// The CEF implementation of ExtensionsClient.
class CefExtensionsClient : public ExtensionsClient {
 public:
  CefExtensionsClient();
  ~CefExtensionsClient() override;

  // ExtensionsClient overrides:
  void Initialize() override;
  const PermissionMessageProvider& GetPermissionMessageProvider()
      const override;
  const std::string GetProductName() override;
  scoped_ptr<FeatureProvider> CreateFeatureProvider(
      const std::string& name) const override;
  scoped_ptr<JSONFeatureProviderSource> CreateFeatureProviderSource(
      const std::string& name) const override;
  void FilterHostPermissions(const URLPatternSet& hosts,
                             URLPatternSet* new_hosts,
                             PermissionIDSet* permissions) const override;
  void SetScriptingWhitelist(const ScriptingWhitelist& whitelist) override;
  const ScriptingWhitelist& GetScriptingWhitelist() const override;
  URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const override;
  bool IsScriptableURL(const GURL& url, std::string* error) const override;
  bool IsAPISchemaGenerated(const std::string& name) const override;
  base::StringPiece GetAPISchema(const std::string& name) const override;
  void RegisterAPISchemaResources(ExtensionAPI* api) const override;
  bool ShouldSuppressFatalErrors() const override;
  void RecordDidSuppressFatalError() override;
  std::string GetWebstoreBaseURL() const override;
  std::string GetWebstoreUpdateURL() const override;
  bool IsBlacklistUpdateURL(const GURL& url) const override;

 private:
  const ExtensionsAPIPermissions extensions_api_permissions_;

  ScriptingWhitelist scripting_whitelist_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionsClient);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_
