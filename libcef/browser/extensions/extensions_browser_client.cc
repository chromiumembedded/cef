// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_browser_client.h"

#include <utility>

#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/extensions/chrome_api_registration.h"
#include "libcef/browser/extensions/component_extension_resource_manager.h"
#include "libcef/browser/extensions/extension_system_factory.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/extensions_api_client.h"

//#include "cef/libcef/browser/extensions/api/generated_api_registration.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_url_request_util.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/generated_api_registration.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/mojo/service_registration.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

CefExtensionsBrowserClient::CefExtensionsBrowserClient()
    : api_client_(new CefExtensionsAPIClient),
      resource_manager_(new CefComponentExtensionResourceManager) {
}

CefExtensionsBrowserClient::~CefExtensionsBrowserClient() {
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
  return CefBrowserContextImpl::GetForContext(context) != NULL;
}

bool CefExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                               BrowserContext* second) {
  // Returns true if |first| and |second| share the same underlying
  // CefBrowserContextImpl.
  return CefBrowserContextImpl::GetForContext(first) ==
         CefBrowserContextImpl::GetForContext(second);
}

bool CefExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* CefExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // TODO(extensions): Do we need to support this?
  return NULL;
}

BrowserContext* CefExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool CefExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
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
  return chrome_url_request_util::MaybeCreateURLRequestResourceBundleJob(
      request,
      network_delegate,
      directory_path,
      content_security_policy,
      send_cors_header);
}

bool CefExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    net::URLRequest* request,
    bool is_incognito,
    const Extension* extension,
    InfoMap* extension_info_map) {
  // TODO(cef): This bypasses additional checks added to
  // AllowCrossRendererResourceLoad() in https://crrev.com/5cf9d45c. Figure out
  // why permission is not being granted based on "web_accessible_resources"
  // specified in the PDF extension manifest.json file.
  if (extension && extension->id() == extension_misc::kPdfExtensionId)
    return true;

  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          request, is_incognito, extension, extension_info_map, &allowed)) {
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
    std::vector<ExtensionPrefsObserver*>* observers) const {
}

ProcessManagerDelegate*
CefExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return NULL;
}

std::unique_ptr<ExtensionHostDelegate>
CefExtensionsBrowserClient::CreateExtensionHostDelegate() {
  // TODO(extensions): Implement to support Apps.
  NOTREACHED();
  return std::unique_ptr<ExtensionHostDelegate>();
}

bool CefExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

void CefExtensionsBrowserClient::PermitExternalProtocolHandler() {
}

bool CefExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool CefExtensionsBrowserClient::IsLoggedInAsPublicAccount()  {
  return false;
}

ExtensionSystemProvider*
CefExtensionsBrowserClient::GetExtensionSystemFactory() {
  return CefExtensionSystemFactory::GetInstance();
}

void CefExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) const {
  // Register core extension-system APIs.
  api::GeneratedFunctionRegistry::RegisterAll(registry);

  // CEF-only APIs.
  // TODO(cef): Enable if/when CEF exposes its own Mojo APIs. See
  // libcef/common/extensions/api/README.txt for details.
  //api::cef::CefGeneratedFunctionRegistry::RegisterAll(registry);

  // Chrome APIs whitelisted by CEF.
  api::cef::ChromeFunctionRegistry::RegisterAll(registry);
}

void CefExtensionsBrowserClient::RegisterMojoServices(
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  RegisterServicesForFrame(render_frame_host, extension);
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
  g_browser_process->extension_event_router_forwarder()->
      BroadcastEventToRenderers(histogram_value, event_name, std::move(args),
                                GURL());
}

net::NetLog* CefExtensionsBrowserClient::GetNetLog() {
  return NULL;
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

void CefExtensionsBrowserClient::SetAPIClientForTest(
    ExtensionsAPIClient* api_client) {
  api_client_.reset(api_client);
}

}  // namespace extensions
