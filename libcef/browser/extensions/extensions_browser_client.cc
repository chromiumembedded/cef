// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_browser_client.h"

#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/extensions/component_extension_resource_manager.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/extensions_api_client.h"
#include "libcef/browser/extensions/extensions_browser_api_provider.h"
#include "libcef/browser/request_context_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_url_request_util.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/core_extensions_browser_api_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/mojo/interface_registration.h"
#include "extensions/browser/serial_extension_host_queue.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

CefExtensionsBrowserClient::CefExtensionsBrowserClient()
    : api_client_(new CefExtensionsAPIClient),
      resource_manager_(new CefComponentExtensionResourceManager) {
  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<CefExtensionsBrowserAPIProvider>());
}

CefExtensionsBrowserClient::~CefExtensionsBrowserClient() {}

// static
CefExtensionsBrowserClient* CefExtensionsBrowserClient::Get() {
  return static_cast<CefExtensionsBrowserClient*>(
      ExtensionsBrowserClient::Get());
}

bool CefExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool CefExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool CefExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  return GetOriginalContext(context) != NULL;
}

bool CefExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                               BrowserContext* second) {
  // Returns true if |first| and |second| share the same underlying
  // CefBrowserContext.
  return GetOriginalContext(first) == GetOriginalContext(second);
}

bool CefExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  // CEF doesn't use incognito contexts.
  return false;
}

BrowserContext* CefExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  return NULL;
}

BrowserContext* CefExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return CefBrowserContext::GetForContext(context);
}

bool CefExtensionsBrowserClient::IsGuestSession(BrowserContext* context) const {
  return false;
}

bool CefExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool CefExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

net::URLRequestJob*
CefExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  NOTREACHED();
  return nullptr;
}

base::FilePath CefExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  return chrome_url_request_util::GetBundleResourcePath(
      request, extension_resources_path, resource_id);
}

void CefExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    network::mojom::URLLoaderRequest loader,
    const base::FilePath& resource_relative_path,
    const int resource_id,
    const std::string& content_security_policy,
    network::mojom::URLLoaderClientPtr client,
    bool send_cors_header) {
  chrome_url_request_util::LoadResourceFromResourceBundle(
      request, std::move(loader), resource_relative_path, resource_id,
      content_security_policy, std::move(client), send_cors_header);
}

bool CefExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const GURL& url,
    content::ResourceType resource_type,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          url, resource_type, page_transition, child_id, is_incognito,
          extension, extensions, process_map, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* CefExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return static_cast<CefBrowserContext*>(context)->GetPrefs();
}

void CefExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {}

ProcessManagerDelegate* CefExtensionsBrowserClient::GetProcessManagerDelegate()
    const {
  return NULL;
}

std::unique_ptr<ExtensionHostDelegate>
CefExtensionsBrowserClient::CreateExtensionHostDelegate() {
  // CEF does not use the ExtensionHost constructor that calls this method.
  NOTREACHED();
  return std::unique_ptr<ExtensionHostDelegate>();
}

bool CefExtensionsBrowserClient::CreateBackgroundExtensionHost(
    const Extension* extension,
    content::BrowserContext* browser_context,
    const GURL& url,
    ExtensionHost** host) {
  CefBrowserContext* browser_context_impl =
      CefBrowserContext::GetForContext(browser_context);

  // A CEF representation should always exist.
  CefRefPtr<CefExtension> cef_extension =
      browser_context_impl->extension_system()->GetExtension(extension->id());
  DCHECK(cef_extension);
  if (!cef_extension) {
    // Cancel the background host creation.
    return true;
  }

  // Always use the same request context that the extension was registered with.
  // GetLoaderContext() will return NULL for internal extensions.
  CefRefPtr<CefRequestContext> request_context =
      cef_extension->GetLoaderContext();
  if (!request_context) {
    // Cancel the background host creation.
    return true;
  }

  CefBrowserHostImpl::CreateParams create_params;
  create_params.url = url;
  create_params.request_context = request_context;

  CefRefPtr<CefExtensionHandler> handler = cef_extension->GetHandler();
  if (handler.get() && handler->OnBeforeBackgroundBrowser(
                           cef_extension, url.spec(), create_params.client,
                           create_params.settings)) {
    // Cancel the background host creation.
    return true;
  }

  // This triggers creation of the background host.
  create_params.extension = extension;
  create_params.extension_host_type =
      extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;

  // Browser creation may fail under certain rare circumstances. Fail the
  // background host creation in that case.
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::Create(create_params);
  if (browser) {
    *host = browser->extension_host();
    DCHECK(*host);
  }
  return true;
}

bool CefExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

void CefExtensionsBrowserClient::PermitExternalProtocolHandler() {}

bool CefExtensionsBrowserClient::IsInDemoMode() {
  return false;
}

bool CefExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
  return false;
}

bool CefExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool CefExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return false;
}

bool CefExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
CefExtensionsBrowserClient::GetExtensionSystemFactory() {
  return CefExtensionSystemFactory::GetInstance();
}

void CefExtensionsBrowserClient::RegisterExtensionInterfaces(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  RegisterInterfacesForExtension(registry, render_frame_host, extension);
}

std::unique_ptr<RuntimeAPIDelegate>
CefExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  // TODO(extensions): Implement to support Apps.
  NOTREACHED();
  return nullptr;
}

const ComponentExtensionResourceManager*
CefExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return resource_manager_.get();
}

void CefExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  g_browser_process->extension_event_router_forwarder()
      ->BroadcastEventToRenderers(histogram_value, event_name, std::move(args),
                                  GURL());
}

ExtensionCache* CefExtensionsBrowserClient::GetExtensionCache() {
  // Only used by Chrome via ExtensionService.
  NOTREACHED();
  return NULL;
}

bool CefExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool CefExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

ExtensionWebContentsObserver*
CefExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return CefExtensionWebContentsObserver::FromWebContents(web_contents);
}

KioskDelegate* CefExtensionsBrowserClient::GetKioskDelegate() {
  NOTREACHED();
  return NULL;
}

bool CefExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
  return false;
}

std::string CefExtensionsBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

ExtensionHostQueue* CefExtensionsBrowserClient::GetExtensionHostQueue() {
  if (!extension_host_queue_)
    extension_host_queue_.reset(new SerialExtensionHostQueue);
  return extension_host_queue_.get();
}

}  // namespace extensions
