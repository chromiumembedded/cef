// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extension_system.h"

#include <string>

#include "libcef/browser/extension_impl.h"
#include "libcef/browser/extensions/pdf_extension_util.h"
#include "libcef/browser/extensions/value_store/cef_value_store_factory.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
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
#include "extensions/browser/state_store.h"
#include "extensions/browser/unloaded_extension_reason.h"
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

// Implementation based on ComponentLoader::ParseManifest.
std::unique_ptr<base::DictionaryValue> ParseManifest(
    const std::string& manifest_contents) {
  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest(
      deserializer.Deserialize(nullptr, nullptr));

  if (!manifest.get() || !manifest->is_dict()) {
    LOG(ERROR) << "Failed to parse extension manifest.";
    return nullptr;
  }
  // Transfer ownership to the caller.
  return base::WrapUnique(
      static_cast<base::DictionaryValue*>(manifest.release()));
}

void ExecuteLoadFailure(CefRefPtr<CefExtensionHandler> handler,
                        cef_errorcode_t result) {
  if (!handler)
    return;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(ExecuteLoadFailure, handler, result));
    return;
  }

  handler->OnExtensionLoadFailed(result);
}

void LoadExtensionOnUIThread(base::WeakPtr<CefExtensionSystem> context,
                             std::unique_ptr<base::DictionaryValue> manifest,
                             const base::FilePath& root_directory,
                             bool internal,
                             CefRefPtr<CefRequestContext> loader_context,
                             CefRefPtr<CefExtensionHandler> handler) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(LoadExtensionOnUIThread, context,
                                          std::move(manifest), root_directory,
                                          internal, loader_context, handler));
    return;
  }

  if (context) {
    context->LoadExtension(std::move(manifest), root_directory, internal,
                           loader_context, handler);
  }
}

void LoadExtensionWithManifest(base::WeakPtr<CefExtensionSystem> context,
                               const std::string& manifest_contents,
                               const base::FilePath& root_directory,
                               bool internal,
                               CefRefPtr<CefRequestContext> loader_context,
                               CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_BLOCKING();

  std::unique_ptr<base::DictionaryValue> manifest =
      ParseManifest(manifest_contents);
  if (!manifest) {
    LOG(WARNING) << "Failed to parse extension manifest";
    ExecuteLoadFailure(handler, ERR_INVALID_ARGUMENT);
    return;
  }

  LoadExtensionOnUIThread(context, std::move(manifest), root_directory,
                          internal, loader_context, handler);
}

void LoadExtensionFromDisk(base::WeakPtr<CefExtensionSystem> context,
                           const base::FilePath& root_directory,
                           bool internal,
                           CefRefPtr<CefRequestContext> loader_context,
                           CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_BLOCKING();

  base::FilePath manifest_path = root_directory.AppendASCII("manifest.json");
  std::string manifest_contents;
  if (!base::ReadFileToString(manifest_path, &manifest_contents)) {
    LOG(WARNING) << "Failed to read extension manifest from "
                 << manifest_path.MaybeAsASCII();
    ExecuteLoadFailure(handler, ERR_FILE_NOT_FOUND);
    return;
  }

  LoadExtensionWithManifest(context, manifest_contents, root_directory,
                            internal, loader_context, handler);
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
  InitPrefs();
}

CefExtensionSystem::~CefExtensionSystem() {}

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

  // Add the internal PDF extension. PDF loading works as follows:
  // 1. The PDF PPAPI plugin is registered in libcef/common/content_client.cc
  //    ComputeBuiltInPlugins to handle the kPDFPluginOutOfProcessMimeType.
  // 2. The PDF extension is registered by the below call to AddExtension and
  //    associated with the "application/pdf" mime type.
  // 3. Web content running in the owner CefBrowser requests to load a PDF file
  //    resource with the "application/pdf" mime type. This can be via a frame
  //    (main frame/iframe) or object/embed tag.
  // 4. PluginResponseInterceptorURLLoaderThrottle intercepts the PDF resource
  //    load in the browser process and registers the PDF resource as a stream
  //    via MimeHandlerStreamManager::AddStream.
  // 5. PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse triggers
  //    creation of a MimeHandlerViewEmbedder in the browser process via
  //    MimeHandlerViewAttachHelper::OverrideBodyForInterceptedResponse.
  // 6. MimeHandlerViewEmbedder::ReadyToCommitNavigation is called and sends a
  //    Mojo message to MimeHandlerViewContainerManager::SetInternalId in the
  //    owner renderer process.
  // 7. The MimeHandlerViewContainerManager is created in the owner renderer
  //    process via MimeHandlerViewContainerManager::BindReceiver and the
  //    SetInternalId call arrives.
  // 8. HTMLPlugInElement::RequestObject is called in the owner renderer process
  //    to handle the PDF file frame/object/embed tag. This results in calls to
  //    ContentBrowserClient::GetPluginMimeTypesWithExternalHandlers (browser
  //    process) and ContentRendererClient::IsPluginHandledExternally (owner
  //    renderer process), and determines that the plugin should be handled
  //    externally (handled_externally=true).
  // 9. MimeHandlerViewContainerManager::IsManagedByContainerManager sends a
  //    Mojo message to MimeHandlerViewEmbedder::ReadyToCreateMimeHandlerView
  //    in the browser process.
  // 10.MimeHandlerViewEmbedder::RenderFrameCreated triggers creation of a
  //    MimeHandlerViewGuest and CefMimeHandlerViewGuestDelegate in the browser
  //    process.
  // 11.MimeHandlerViewGuest::CreateWebContents creates a new guest WebContents
  //    (is_guest_view=true) to host the PDF extension and the PDF resource
  //    stream is retrieved via MimeHandlerStreamManager::ReleaseStream.
  // 12.MimeHandlerViewGuest::DidAttachToEmbedder calls
  //    CefMimeHandlerViewGuestDelegate::OnGuestAttached to associate the guest
  //    WebContents routing IDs with the owner CefBrowser. MimeHandlerViewGuest
  //    then loads the extension URL (index.html) in the guest WebContents.
  // 13.Creation of the RenderFrame in the guest renderer process triggers a
  //    sync IPC call from CefContentRendererClient::MaybeCreateBrowser to
  //    CefBrowserInfoManager::GetBrowserInfo in the browser process to retrieve
  //    the CefBrowser information, which will be immediately available due to
  //    step 12.
  // 14.The PDF extension begins to load. Extension resource requests are
  //    handled via ExtensionURLLoaderFactory::CreateLoaderAndStart in the
  //    browser process. Access to PDF extension resources is checked by
  //    CefExtensionsBrowserClient::AllowCrossRendererResourceLoad and
  //    PDF extension resources are provided from bundle via
  //    CefExtensionsBrowserClient::LoadResourceFromResourceBundle
  //    and CefComponentExtensionResourceManager. Access to chrome://resources
  //    is granted via CefExtensionWebContentsObserver::RenderViewCreated.
  // 15.The PDF extension calls chrome.mimeHandlerPrivate.getStreamInfo
  //    (chrome/browser/resources/pdf/browser_api.js) to retrieve the PDF
  //    resource stream. This API is implemented using Mojo as described in
  //    libcef/common/extensions/api/README.txt.
  // 16.The PDF extension requests the PDF PPAPI plugin to handle
  //    kPDFPluginOutOfProcessMimeType. Approval arrives in the guest renderer
  //    process via ExtensionFrameHelper::OnExtensionResponse which calls
  //    NativeExtensionBindingsSystem::HandleResponse. This triggers creation of
  //    an HTMLPlugInElement via native V8 bindings to host the PDF plugin.
  // 17.HTMLPlugInElement::RequestObject is called in the guest renderer process
  //    and determines that the PDF PPAPI plugin should be handled internally
  //    (handled_externally=false). A PluginDocument is created and
  //    CefContentRendererClient::OverrideCreatePlugin is called to create a
  //    WebPlugin.
  // 18.The PDF extension and PDF plugin are now loaded. Print commands, if
  //    any, are handled in the guest renderer process by ChromePDFPrintClient
  //    and CefPrintRenderFrameHelperDelegate.
  // 19.When navigating away from the PDF file or closing the owner CefBrowser
  //    the guest WebContents will be destroyed. This triggers a call to
  //    CefMimeHandlerViewGuestDelegate::OnGuestDetached which removes the
  //    routing ID association with the owner CefBrowser.
  if (PdfExtensionEnabled()) {
    LoadExtension(ParseManifest(pdf_extension_util::GetManifest()),
                  base::FilePath(FILE_PATH_LITERAL("pdf")), true /* internal */,
                  nullptr, nullptr);
  }

  initialized_ = true;
}

void CefExtensionSystem::LoadExtension(
    const base::FilePath& root_directory,
    bool internal,
    CefRefPtr<CefRequestContext> loader_context,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  CEF_POST_USER_VISIBLE_TASK(
      base::BindOnce(LoadExtensionFromDisk, weak_ptr_factory_.GetWeakPtr(),
                     root_directory, internal, loader_context, handler));
}

void CefExtensionSystem::LoadExtension(
    const std::string& manifest_contents,
    const base::FilePath& root_directory,
    bool internal,
    CefRefPtr<CefRequestContext> loader_context,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(
      LoadExtensionWithManifest, weak_ptr_factory_.GetWeakPtr(),
      manifest_contents, root_directory, internal, loader_context, handler));
}

// Implementation based on ComponentLoader::Add.
void CefExtensionSystem::LoadExtension(
    std::unique_ptr<base::DictionaryValue> manifest,
    const base::FilePath& root_directory,
    bool internal,
    CefRefPtr<CefRequestContext> loader_context,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();

// Internal extensions don't have a loader context. External extensions should.
#if DCHECK_IS_ON()
  if (internal) {
    DCHECK(!loader_context);
  } else {
    DCHECK(loader_context);
  }
#endif

  ComponentExtensionInfo info(manifest.get(), root_directory, internal);
  const Extension* extension = LoadExtension(info, loader_context, handler);
  if (!extension)
    ExecuteLoadFailure(handler, ERR_FAILED);
}

// Implementation based on ExtensionService::RemoveComponentExtension.
bool CefExtensionSystem::UnloadExtension(const std::string& extension_id) {
  CEF_REQUIRE_UIT();
  ExtensionMap::iterator it = extension_map_.find(extension_id);
  if (it == extension_map_.end()) {
    // No CEF representation so we've already unloaded it.
    return false;
  }

  CefRefPtr<CefExtensionImpl> cef_extension =
      static_cast<CefExtensionImpl*>(it->second.get());

  // Erase first so that callbacks can't retrieve the unloaded extension.
  extension_map_.erase(it);

  cef_extension->OnExtensionUnloaded();

  scoped_refptr<const Extension> extension(
      registry_->GetInstalledExtension(extension_id));
  UnloadExtension(extension_id, UnloadedExtensionReason::UNINSTALL);
  if (extension.get()) {
    registry_->TriggerOnUninstalled(
        extension.get(), extensions::UNINSTALL_REASON_COMPONENT_REMOVED);
  }

  return true;
}

bool CefExtensionSystem::HasExtension(const std::string& extension_id) const {
  return !!GetExtension(extension_id);
}

CefRefPtr<CefExtension> CefExtensionSystem::GetExtension(
    const std::string& extension_id) const {
  CEF_REQUIRE_UIT();
  ExtensionMap::const_iterator it = extension_map_.find(extension_id);
  if (it != extension_map_.end())
    return it->second;
  return nullptr;
}

CefExtensionSystem::ExtensionMap CefExtensionSystem::GetExtensions() const {
  CEF_REQUIRE_UIT();
  return extension_map_;
}

void CefExtensionSystem::OnRequestContextDeleted(CefRequestContext* context) {
  CEF_REQUIRE_UIT();
  DCHECK(context);

  // Make a copy of the map because UnloadExtension will modify it.
  // Don't add any references to |context|.
  ExtensionMap map = extension_map_;
  ExtensionMap::const_iterator it = map.begin();
  for (; it != map.end(); ++it) {
    CefRefPtr<CefExtensionImpl> cef_extension =
        static_cast<CefExtensionImpl*>(it->second.get());
    if (cef_extension->loader_context() == context)
      UnloadExtension(it->first);
  }
}

void CefExtensionSystem::Shutdown() {
  CEF_REQUIRE_UIT();
// Only internal extensions should exist at this point.
#if DCHECK_IS_ON()
  ExtensionMap::iterator it = extension_map_.begin();
  for (; it != extension_map_.end(); ++it) {
    CefRefPtr<CefExtensionImpl> cef_extension =
        static_cast<CefExtensionImpl*>(it->second.get());
    DCHECK(!cef_extension->loader_context());
  }
#endif
  extension_map_.clear();
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
  return state_store_.get();
}

StateStore* CefExtensionSystem::rules_store() {
  return rules_store_.get();
}

scoped_refptr<ValueStoreFactory> CefExtensionSystem::store_factory() {
  return store_factory_;
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
    base::OnceClosure callback) {
  // TODO(extensions): The |incognito_enabled| value should be set based on
  // manifest settings.
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&InfoMap::AddExtension, info_map(),
                 base::RetainedRef(extension), base::Time::Now(),
                 true,    // incognito_enabled
                 false),  // notifications_disabled
      std::move(callback));
}

// Implementation based on
// ExtensionSystemImpl::UnregisterExtensionWithRequestContexts.
void CefExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionReason reason) {
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&InfoMap::RemoveExtension, info_map(), extension_id, reason));
}

const base::OneShotEvent& CefExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* CefExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> CefExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return std::make_unique<ExtensionSet>();
}

void CefExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
  base::DeleteFile(temp_dir, true /* recursive */);
}

bool CefExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
  return false;
}

CefExtensionSystem::ComponentExtensionInfo::ComponentExtensionInfo(
    const base::DictionaryValue* manifest,
    const base::FilePath& directory,
    bool internal)
    : manifest(manifest), root_directory(directory), internal(internal) {
  if (!root_directory.IsAbsolute()) {
    // This path structure is required by
    // url_request_util::MaybeCreateURLRequestResourceBundleJob.
    CHECK(base::PathService::Get(chrome::DIR_RESOURCES, &root_directory));
    root_directory = root_directory.Append(directory);
  }
}

void CefExtensionSystem::InitPrefs() {
  store_factory_ = new CefValueStoreFactory(browser_context_->GetPath());

  Profile* profile = Profile::FromBrowserContext(browser_context_);

  // Two state stores. The latter, which contains declarative rules, must be
  // loaded immediately so that the rules are ready before we issue network
  // requests.
  state_store_.reset(new StateStore(
      profile, store_factory_, ValueStoreFrontend::BackendType::STATE, true));

  rules_store_.reset(new StateStore(
      profile, store_factory_, ValueStoreFrontend::BackendType::RULES, false));
}

// Implementation based on ComponentLoader::CreateExtension.
scoped_refptr<const Extension> CefExtensionSystem::CreateExtension(
    const ComponentExtensionInfo& info,
    std::string* utf8_error) {
  // TODO(abarth): We should REQUIRE_MODERN_MANIFEST_VERSION once we've updated
  //               our component extensions to the new manifest version.
  int flags = 0;
  if (info.internal) {
    // Internal extensions must have kPublicKey in the manifest.
    flags |= Extension::REQUIRE_KEY;
  }
  return Extension::Create(
      info.root_directory,
      // Tests should continue to use the Manifest::COMMAND_LINE value here
      // Some Chrome APIs will cause undesired effects if this is incorrect
      // e.g.: alarms API has 1 minute minimum applied to Packed Extensions
      info.internal ? Manifest::COMPONENT : Manifest::COMMAND_LINE,
      *info.manifest, flags, utf8_error);
}

// Implementation based on ComponentLoader::Load and
// ExtensionService::AddExtension.
const Extension* CefExtensionSystem::LoadExtension(
    const ComponentExtensionInfo& info,
    CefRefPtr<CefRequestContext> loader_context,
    CefRefPtr<CefExtensionHandler> handler) {
  std::string error;
  scoped_refptr<const Extension> extension(CreateExtension(info, &error));
  if (!extension.get()) {
    LOG(ERROR) << error;
    return nullptr;
  }

  if (registry_->GetInstalledExtension(extension->id())) {
    LOG(ERROR) << "Extension with id " << extension->id()
               << "is already installed";
    return nullptr;
  }

  CefRefPtr<CefExtensionImpl> cef_extension =
      new CefExtensionImpl(extension.get(), loader_context.get(), handler);

  // Insert first so that callbacks can retrieve the loaded extension.
  extension_map_.insert(std::make_pair(extension->id(), cef_extension));

  cef_extension->OnExtensionLoaded();

  // This may trigger additional callbacks.
  registry_->AddEnabled(extension.get());
  NotifyExtensionLoaded(extension.get());

  return extension.get();
}

// Implementation based on ExtensionService::UnloadExtension.
void CefExtensionSystem::UnloadExtension(const std::string& extension_id,
                                         UnloadedExtensionReason reason) {
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
                 base::WrapRefCounted(extension)));

  // Tell renderers about the loaded extension.
  renderer_helper_->OnExtensionLoaded(*extension);

  // Tell subsystems that use the ExtensionRegistryObserver::OnExtensionLoaded
  // about the new extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // ExtensionRegistryObserver::OnExtensionLoaded the renderer is guaranteed to
  // know about it.
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
    UnloadedExtensionReason reason) {
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

  // Tell renderers about the unloaded extension.
  renderer_helper_->OnExtensionUnloaded(*extension);

  UnregisterExtensionWithRequestContexts(extension->id(), reason);
}

}  // namespace extensions
