// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_
#define CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/common/extensions_client.h"
#include "url/gurl.h"

namespace extensions {

// The CEF implementation of ExtensionsClient.
class CefExtensionsClient : public ExtensionsClient {
 public:
  CefExtensionsClient();

  CefExtensionsClient(const CefExtensionsClient&) = delete;
  CefExtensionsClient& operator=(const CefExtensionsClient&) = delete;

  ~CefExtensionsClient() override;

  // ExtensionsClient overrides:
  void Initialize() override;
  void InitializeWebStoreUrls(base::CommandLine* command_line) override;
  const PermissionMessageProvider& GetPermissionMessageProvider()
      const override;
  const std::string GetProductName() override;
  void FilterHostPermissions(const URLPatternSet& hosts,
                             URLPatternSet* new_hosts,
                             PermissionIDSet* permissions) const override;
  void SetScriptingAllowlist(const ScriptingAllowlist& allowlist) override;
  const ScriptingAllowlist& GetScriptingAllowlist() const override;
  URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const override;
  bool IsScriptableURL(const GURL& url, std::string* error) const override;
  const GURL& GetWebstoreBaseURL() const override;
  const GURL& GetNewWebstoreBaseURL() const override;
  const GURL& GetWebstoreUpdateURL() const override;
  bool IsBlocklistUpdateURL(const GURL& url) const override;

 private:
  const ChromePermissionMessageProvider permission_message_provider_;

  ScriptingAllowlist scripting_allowlist_;

  // Mutable to allow caching in a const method.
  const GURL webstore_base_url_;
  const GURL new_webstore_base_url_;
  const GURL webstore_update_url_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_CLIENT_H_
