// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

class ExtensionsAPIClient;

// An ExtensionsBrowserClient that supports a single content::BrowserContent
// with no related incognito context.
class CefExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  CefExtensionsBrowserClient();
  ~CefExtensionsBrowserClient() override;

  // Returns the singleton CefExtensionsBrowserClient instance.
  static CefExtensionsBrowserClient* Get();

  // ExtensionsBrowserClient overrides:
  bool IsShuttingDown() override;
  bool AreExtensionsDisabled(const base::CommandLine& command_line,
                             content::BrowserContext* context) override;
  bool IsValidContext(content::BrowserContext* context) override;
  bool IsSameContext(content::BrowserContext* first,
                     content::BrowserContext* second) override;
  bool HasOffTheRecordContext(content::BrowserContext* context) override;
  content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
  bool IsGuestSession(content::BrowserContext* context) const override;
  bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  bool CanExtensionCrossIncognito(
      const Extension* extension,
      content::BrowserContext* context) const override;
  base::FilePath GetBundleResourcePath(
      const network::ResourceRequest& request,
      const base::FilePath& extension_resources_path,
      int* resource_id) const override;
  void LoadResourceFromResourceBundle(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const base::FilePath& resource_relative_path,
      const int resource_id,
      const std::string& content_security_policy,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool send_cors_header) override;
  bool AllowCrossRendererResourceLoad(const GURL& url,
                                      content::ResourceType resource_type,
                                      ui::PageTransition page_transition,
                                      int child_id,
                                      bool is_incognito,
                                      const Extension* extension,
                                      const ExtensionSet& extensions,
                                      const ProcessMap& process_map) override;
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const override;
  ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate() override;
  bool CreateBackgroundExtensionHost(const Extension* extension,
                                     content::BrowserContext* browser_context,
                                     const GURL& url,
                                     ExtensionHost** host) override;
  bool DidVersionUpdate(content::BrowserContext* context) override;
  void PermitExternalProtocolHandler() override;
  bool IsInDemoMode() override;
  bool IsScreensaverInDemoMode(const std::string& app_id) override;
  bool IsRunningInForcedAppMode() override;
  bool IsAppModeForcedForApp(const ExtensionId& extension_id) override;
  bool IsLoggedInAsPublicAccount() override;
  ExtensionSystemProvider* GetExtensionSystemFactory() override;
  void RegisterBrowserInterfaceBindersForFrame(
      service_manager::BinderMapWithContext<content::RenderFrameHost*>*
          binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const override;
  std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args,
      bool dispatch_to_off_the_record_profiles) override;
  ExtensionCache* GetExtensionCache() override;
  bool IsBackgroundUpdateAllowed() override;
  bool IsMinBrowserVersionSupported(const std::string& min_version) override;
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  KioskDelegate* GetKioskDelegate() override;
  bool IsLockScreenContext(content::BrowserContext* context) override;
  std::string GetApplicationLocale() override;

 private:
  // Support for extension APIs.
  std::unique_ptr<ExtensionsAPIClient> api_client_;

  // Resource manager used to supply resources from pak files.
  std::unique_ptr<ComponentExtensionResourceManager> resource_manager_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_BROWSER_CLIENT_H_
