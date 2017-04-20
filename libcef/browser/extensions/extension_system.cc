// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extension_system.h"

#include <string>

#include "libcef/browser/extensions/pdf_extension_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/value_store/value_store_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/switches.h"
#include "net/base/mime_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

std::string GenerateId(const base::DictionaryValue* manifest,
                       const base::FilePath& path) {
  std::string raw_key;
  std::string id_input;
  CHECK(manifest->GetString(manifest_keys::kPublicKey, &raw_key));
  CHECK(Extension::ParsePEMKeyBytes(raw_key, &id_input));
  std::string id = crx_file::id_util::GenerateId(id_input);
  return id;
}

// Implementation based on ComponentLoader::ParseManifest.
base::DictionaryValue* ParseManifest(
    const std::string& manifest_contents) {
  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest(deserializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->IsType(base::Value::Type::DICTIONARY)) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return NULL;
  }
  // Transfer ownership to the caller.
  return static_cast<base::DictionaryValue*>(manifest.release());
}

}  // namespace

CefExtensionSystem::CefExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context),
      initialized_(false),
      registry_(ExtensionRegistry::Get(browser_context)),
      renderer_helper_(
          extensions::RendererStartupHelperFactory::GetForBrowserContext(
              browser_context)),
      weak_ptr_factory_(this) {
}

CefExtensionSystem::~CefExtensionSystem() {
}

void CefExtensionSystem::Init() {
  DCHECK(!initialized_);

  // There's complexity here related to the ordering of message delivery. For
  // an extension to load correctly both the ExtensionMsg_Loaded and
  // ExtensionMsg_ActivateExtension messages must be sent. These messages are
  // currently sent by RendererStartupHelper, ExtensionWebContentsObserver, and
  // this class. ExtensionMsg_Loaded is handled by Dispatcher::OnLoaded and adds
  // the extension to |extensions_|. ExtensionMsg_ActivateExtension is handled
  // by Dispatcher::OnActivateExtension and adds the extension to
  // |active_extension_ids_|. If these messages are not sent correctly then
  // ScriptContextSet::Register called from Dispatcher::DidCreateScriptContext
  // will classify the extension incorrectly and API bindings will not be added.

  // Inform the rest of the extensions system to start.
  ready_.Signal();
  content::NotificationService::current()->Notify(
      NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::NotificationService::NoDetails());

  // Add the built-in PDF extension. PDF loading works as follows:
  // 1. PDF PPAPI plugin is registered to handle kPDFPluginOutOfProcessMimeType
  //    in libcef/common/content_client.cc ComputeBuiltInPlugins.
  // 2. PDF extension is registered and associated with the "application/pdf"
  //    mime type by the below call to AddExtension.
  // 3. A page requests a resource with the "application/pdf" mime type. For
  //    example, by loading a PDF file.
  // 4. CefResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream
  //    intercepts the PDF resource load in the browser process, generates a
  //    unique View ID that is associated with the resource request for later
  //    retrieval via MimeHandlerStreamManager and the
  //    chrome.mimeHandlerPrivate JS API (extensions/common/api/
  //    mime_handler_private.idl), and returns the unique View ID via the
  //    |payload| argument.
  // 5. The unique View ID arrives in the renderer process via
  //    ResourceLoader::didReceiveData and triggers creation of a new Document.
  //    DOMImplementation::createDocument indirectly calls
  //    RendererBlinkPlatformImpl::getPluginList to retrieve the list of
  //    supported plugins from the browser process. If a plugin supports the
  //    "application/pdf" mime type then a PluginDocument is created and
  //    CefContentRendererClient::OverrideCreatePlugin is called. This then
  //    indirectly calls CefContentRendererClient::CreateBrowserPluginDelegate
  //    to create a MimeHandlerViewContainer.
  // 6. A MimeHandlerViewGuest and CefMimeHandlerViewGuestDelegate is created in
  //    the browser process.
  // 7. MimeHandlerViewGuest navigates to the PDF extension URL.
  // 8. Access to PDF extension resources is checked by
  //    CefExtensionsBrowserClient::AllowCrossRendererResourceLoad.
  // 9. PDF extension resources are provided from bundle via
  //    CefExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob and
  //    CefComponentExtensionResourceManager.
  // 10.The PDF extension (chrome/browser/resources/pdf/browser_api.js) calls
  //    chrome.mimeHandlerPrivate.getStreamInfo to retrieve the PDF resource
  //    stream. This API is implemented using Mojo as described in
  //    libcef/common/extensions/api/README.txt.
  // 11.The PDF extension requests a plugin to handle
  //    kPDFPluginOutOfProcessMimeType which loads the PDF PPAPI plugin.
  // 12.Routing of print-related commands are handled by ChromePDFPrintClient
  //    and CefPrintWebViewHelperDelegate in the renderer process.
  // 13.The PDF extension is granted access to chrome://resources via
  //    CefExtensionWebContentsObserver::RenderViewCreated in the browser
  //    process.
  if (PdfExtensionEnabled()) {
    AddExtension(pdf_extension_util::GetManifest(),
                 base::FilePath(FILE_PATH_LITERAL("pdf")));
  }

  initialized_ = true;
}

// Implementation based on ComponentLoader::Add.
const Extension* CefExtensionSystem::AddExtension(
    const std::string& manifest_contents,
    const base::FilePath& root_directory) {
  base::DictionaryValue* manifest = ParseManifest(manifest_contents);
  if (!manifest)
    return NULL;

  ComponentExtensionInfo info(manifest, root_directory);
  const Extension* extension = LoadExtension(info);
  delete manifest;

  return extension;
}

// Implementation based on ExtensionService::RemoveComponentExtension.
void CefExtensionSystem::RemoveExtension(const std::string& extension_id) {
  scoped_refptr<const Extension> extension(
      registry_->enabled_extensions().GetByID(extension_id));
  UnloadExtension(extension_id, UnloadedExtensionInfo::REASON_UNINSTALL);
  if (extension.get()) {
    registry_->TriggerOnUninstalled(
        extension.get(), extensions::UNINSTALL_REASON_COMPONENT_REMOVED);
  }
}

void CefExtensionSystem::Shutdown() {
}

void CefExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  DCHECK(!initialized_);
  service_worker_manager_.reset(new ServiceWorkerManager(browser_context_));
  runtime_data_.reset(new RuntimeData(registry_));
  quota_service_.reset(new QuotaService);
  app_sorting_.reset(new NullAppSorting);
}

ExtensionService* CefExtensionSystem::extension_service() {
  return nullptr;
}

RuntimeData* CefExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* CefExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* CefExtensionSystem::service_worker_manager() {
  return service_worker_manager_.get();
}

SharedUserScriptMaster* CefExtensionSystem::shared_user_script_master() {
  return nullptr;
}

StateStore* CefExtensionSystem::state_store() {
  return nullptr;
}

StateStore* CefExtensionSystem::rules_store() {
  return nullptr;
}

scoped_refptr<ValueStoreFactory> CefExtensionSystem::store_factory() {
  return nullptr;
}

InfoMap* CefExtensionSystem::info_map() {
  if (!info_map_.get())
    info_map_ = new InfoMap;
  return info_map_.get();
}

QuotaService* CefExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* CefExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

// Implementation based on
// ExtensionSystemImpl::RegisterExtensionWithRequestContexts.
void CefExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension,
    const base::Closure& callback) {
  // TODO(extensions): The |incognito_enabled| value should be set based on
  // manifest settings.
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::AddExtension,
                  info_map(),
                  base::RetainedRef(extension),
                  base::Time::Now(),
                  true,     // incognito_enabled
                  false),   // notifications_disabled
      callback);
}

// Implementation based on
// ExtensionSystemImpl::UnregisterExtensionWithRequestContexts.
void CefExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&InfoMap::RemoveExtension, info_map(), extension_id, reason));
}

const OneShotEvent& CefExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* CefExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> CefExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return base::MakeUnique<ExtensionSet>();
}

void CefExtensionSystem::InstallUpdate(const std::string& extension_id,
                                       const base::FilePath& temp_dir) {
  NOTREACHED();
  base::DeleteFile(temp_dir, true /* recursive */);
}


CefExtensionSystem::ComponentExtensionInfo::ComponentExtensionInfo(
    const base::DictionaryValue* manifest, const base::FilePath& directory)
    : manifest(manifest),
      root_directory(directory) {
  if (!root_directory.IsAbsolute()) {
    // This path structure is required by
    // url_request_util::MaybeCreateURLRequestResourceBundleJob.
    CHECK(PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
  extension_id = GenerateId(manifest, root_directory);
}

// Implementation based on ComponentLoader::CreateExtension.
scoped_refptr<const Extension> CefExtensionSystem::CreateExtension(
    const ComponentExtensionInfo& info, std::string* utf8_error) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = Extension::REQUIRE_KEY;
  return Extension::Create(
      info.root_directory,
      Manifest::COMPONENT,
      *info.manifest,
      flags,
      utf8_error);
}

// Implementation based on ComponentLoader::Load and
// ExtensionService::AddExtension.
const Extension* CefExtensionSystem::LoadExtension(
    const ComponentExtensionInfo& info) {
  std::string error;
  scoped_refptr<const Extension> extension(CreateExtension(info, &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return NULL;
  }

  CHECK_EQ(info.extension_id, extension->id()) << extension->name();

  registry_->AddEnabled(extension.get());
  NotifyExtensionLoaded(extension.get());

  return extension.get();
}

// Implementation based on ExtensionService::UnloadExtension.
void CefExtensionSystem::UnloadExtension(
    const std::string& extension_id,
    UnloadedExtensionInfo::Reason reason) {
  // Make sure the extension gets deleted after we return from this function.
  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // This method can be called via PostTask, so the extension may have been
  // unloaded by the time this runs.
  if (!extension.get()) {
    // In case the extension may have crashed/uninstalled. Allow the profile to
    // clean up its RequestContexts.
    UnregisterExtensionWithRequestContexts(extension_id, reason);
    return;
  }

  if (registry_->disabled_extensions().Contains(extension->id())) {
    registry_->RemoveDisabled(extension->id());
    // Make sure the profile cleans up its RequestContexts when an already
    // disabled extension is unloaded (since they are also tracking the disabled
    // extensions).
    UnregisterExtensionWithRequestContexts(extension_id, reason);
    // Don't send the unloaded notification. It was sent when the extension
    // was disabled.
  } else {
    // Remove the extension from the enabled list.
    registry_->RemoveEnabled(extension->id());
    NotifyExtensionUnloaded(extension.get(), reason);
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));
}

// Implementation based on ExtensionService::NotifyExtensionLoaded.
void CefExtensionSystem::NotifyExtensionLoaded(const Extension* extension) {
  // The URLRequestContexts need to be first to know that the extension
  // was loaded, otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about. The profile is responsible
  // for ensuring its URLRequestContexts appropriately discover the loaded
  // extension.
  RegisterExtensionWithRequestContexts(
      extension,
      base::Bind(&CefExtensionSystem::OnExtensionRegisteredWithRequestContexts,
                 weak_ptr_factory_.GetWeakPtr(),
                 make_scoped_refptr(extension)));

  // Tell renderers about the loaded extension.
  renderer_helper_->OnExtensionLoaded(*extension);

  // Tell subsystems that use the EXTENSION_LOADED notification about the new
  // extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // ExtensionRegistryObserver::OnLoaded or
  // NOTIFICATION_EXTENSION_LOADED_DEPRECATED, the
  // renderer is guaranteed to know about it.
  registry_->TriggerOnLoaded(extension);

  // Register plugins included with the extension.
  // Implementation based on PluginManager::OnExtensionLoaded.
  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && !handler->handler_url().empty()) {
    content::WebPluginInfo info;
    info.type = content::WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN;
    info.name = base::UTF8ToUTF16(extension->name());
    info.path = base::FilePath::FromUTF8Unsafe(extension->url().spec());

    for (std::set<std::string>::const_iterator mime_type =
         handler->mime_type_set().begin();
         mime_type != handler->mime_type_set().end(); ++mime_type) {
      content::WebPluginMimeType mime_type_info;
      mime_type_info.mime_type = *mime_type;
      base::FilePath::StringType file_extension;
      if (net::GetPreferredExtensionForMimeType(*mime_type, &file_extension)) {
        mime_type_info.file_extensions.push_back(
            base::FilePath(file_extension).AsUTF8Unsafe());
      }
      info.mime_types.push_back(mime_type_info);
    }
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RefreshPlugins();
    plugin_service->RegisterInternalPlugin(info, true);
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension));
}

void CefExtensionSystem::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<const extensions::Extension> extension) {
  registry_->AddReady(extension);
  if (registry_->enabled_extensions().Contains(extension->id()))
    registry_->TriggerOnReady(extension.get());
}

// Implementation based on ExtensionService::NotifyExtensionUnloaded.
void CefExtensionSystem::NotifyExtensionUnloaded(
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  UnloadedExtensionInfo details(extension, reason);

  // Unregister plugins included with the extension.
  // Implementation based on PluginManager::OnExtensionUnloaded.
  const MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension);
  if (handler && !handler->handler_url().empty()) {
    base::FilePath path =
        base::FilePath::FromUTF8Unsafe(extension->url().spec());
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->UnregisterInternalPlugin(path);
    plugin_service->RefreshPlugins();
  }

  registry_->TriggerOnUnloaded(extension, reason);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<UnloadedExtensionInfo>(&details));

  // Tell renderers about the unloaded extension.
  renderer_helper_->OnExtensionUnloaded(*extension);

  UnregisterExtensionWithRequestContexts(extension->id(), reason);
}

}  // namespace extensions
