// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_webkit_init.h"
#include "browser_resource_loader_bridge.h"
#include "browser_socket_stream_bridge.h"
#include "browser_webkit_glue.h"
#include "browser_webstoragenamespace_impl.h"
#include "cef_context.h"

#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "media/base/media.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBKeyPath.h"
#include "v8/include/v8.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"


BrowserWebKitInit::BrowserWebKitInit()
    : clipboard_(&clipboard_client_) {
  v8::V8::SetCounterFunction(base::StatsTable::FindLocation);

  WebKit::initialize(this);
  WebKit::setLayoutTestMode(false);
  WebKit::WebRuntimeFeatures::enableSockets(true);
  WebKit::WebRuntimeFeatures::enableApplicationCache(true);
  WebKit::WebRuntimeFeatures::enableDatabase(true);
  WebKit::WebRuntimeFeatures::enablePushState(true);
  WebKit::WebRuntimeFeatures::enableIndexedDatabase(true);
  WebKit::WebRuntimeFeatures::enableFileSystem(true);
  WebKit::WebRuntimeFeatures::enableGeolocation(true);

  // TODO: Enable these once the implementation supports it.
  WebKit::WebRuntimeFeatures::enableNotifications(false);
  WebKit::WebRuntimeFeatures::enableSpeechInput(false);
  WebKit::WebRuntimeFeatures::enableTouch(false);
  WebKit::WebRuntimeFeatures::enableDeviceMotion(false);
  WebKit::WebRuntimeFeatures::enableDeviceOrientation(false);

  // Load libraries for media and enable the media player.
  FilePath module_path;
  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      PathService::Get(base::DIR_MODULE, &module_path) &&
      media::InitializeMediaLibrary(module_path));

  // Construct and initialize an appcache system for this scope.
  // A new empty temp directory is created to house any cached
  // content during the run. Upon exit that directory is deleted.
  // If we can't create a tempdir, we'll use in-memory storage.
  if (!appcache_dir_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the appcache, "
                    "using in-memory storage.";
    DCHECK(appcache_dir_.path().empty());
  }
  BrowserAppCacheSystem::InitializeOnUIThread(appcache_dir_.path());

  WebKit::WebDatabase::setObserver(&database_system_);

  blob_registry_ = new BrowserWebBlobRegistryImpl();

  file_utilities_.set_sandbox_enabled(sandboxEnabled());
}

BrowserWebKitInit::~BrowserWebKitInit() {
  WebKit::shutdown();
}

WebKit::WebMimeRegistry* BrowserWebKitInit::mimeRegistry() {
  return &mime_registry_;
}

WebKit::WebClipboard* BrowserWebKitInit::clipboard() {
  return &clipboard_;
}

WebKit::WebFileUtilities* BrowserWebKitInit::fileUtilities() {
  return &file_utilities_;
}

WebKit::WebSandboxSupport* BrowserWebKitInit::sandboxSupport() {
  return NULL;
}

WebKit::WebBlobRegistry* BrowserWebKitInit::blobRegistry() {
  return blob_registry_.get();
}

WebKit::WebCookieJar* BrowserWebKitInit::cookieJar() {
  return &cookie_jar_;
}

WebKit::WebFileSystem* BrowserWebKitInit::fileSystem() {
  BrowserFileSystem* file_system = _Context->file_system();
  // Create the context if it doesn't already exist.
  file_system->CreateContext();
  return file_system;
}

bool BrowserWebKitInit::sandboxEnabled() {
  return false;
}

WebKit::WebKitPlatformSupport::FileHandle
BrowserWebKitInit::databaseOpenFile(
    const WebKit::WebString& vfs_file_name, int desired_flags) {
  return BrowserDatabaseSystem::GetInstance()->OpenFile(
      vfs_file_name, desired_flags);
}

int BrowserWebKitInit::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  return BrowserDatabaseSystem::GetInstance()->DeleteFile(
      vfs_file_name, sync_dir);
}

long BrowserWebKitInit::databaseGetFileAttributes(
    const WebKit::WebString& vfs_file_name) {
  return BrowserDatabaseSystem::GetInstance()->GetFileAttributes(
      vfs_file_name);
}

long long BrowserWebKitInit::databaseGetFileSize(
    const WebKit::WebString& vfs_file_name) {
  return BrowserDatabaseSystem::GetInstance()->GetFileSize(vfs_file_name);
}

long long BrowserWebKitInit::databaseGetSpaceAvailableForOrigin(
    const WebKit::WebString& origin_identifier) {
  return BrowserDatabaseSystem::GetInstance()->GetSpaceAvailable(
      origin_identifier);
}

unsigned long long BrowserWebKitInit::visitedLinkHash(
    const char* canonicalURL, size_t length) {
  return 0;
}

bool BrowserWebKitInit::isLinkVisited(unsigned long long linkHash) {
  return false;
}

WebKit::WebMessagePortChannel*
BrowserWebKitInit::createMessagePortChannel() {
  return NULL;
}

void BrowserWebKitInit::prefetchHostName(const WebKit::WebString&) {
}

void BrowserWebKitInit::decrementStatsCounter(const char* name) {
}

void BrowserWebKitInit::incrementStatsCounter(const char* name) {
}

void BrowserWebKitInit::histogramCustomCounts(const char* name, int sample,
                                              int min, int max,
                                              int bucket_count) {
}

void BrowserWebKitInit::histogramEnumeration(const char* name, int sample,
                                             int boundary_value) {
}

bool BrowserWebKitInit::isTraceEventEnabled() const {
  return false;
}

void BrowserWebKitInit::traceEventBegin(const char* name, void* id,
                                        const char* extra) {
}

void BrowserWebKitInit::traceEventEnd(const char* name, void* id, 
                                      const char* extra) {
}

WebKit::WebData BrowserWebKitInit::loadResource(const char* name) {
  if (!strcmp(name, "deleteButton")) {
    // Create a red 30x30 square.
    const char red_square[] =
        "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
        "\x00\x00\x00\x1e\x00\x00\x00\x1e\x04\x03\x00\x00\x00\xc9\x1e\xb3"
        "\x91\x00\x00\x00\x30\x50\x4c\x54\x45\x00\x00\x00\x80\x00\x00\x00"
        "\x80\x00\x80\x80\x00\x00\x00\x80\x80\x00\x80\x00\x80\x80\x80\x80"
        "\x80\xc0\xc0\xc0\xff\x00\x00\x00\xff\x00\xff\xff\x00\x00\x00\xff"
        "\xff\x00\xff\x00\xff\xff\xff\xff\xff\x7b\x1f\xb1\xc4\x00\x00\x00"
        "\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00\x00\x0b\x13\x01\x00\x9a"
        "\x9c\x18\x00\x00\x00\x17\x49\x44\x41\x54\x78\x01\x63\x98\x89\x0a"
        "\x18\x50\xb9\x33\x47\xf9\xa8\x01\x32\xd4\xc2\x03\x00\x33\x84\x0d"
        "\x02\x3a\x91\xeb\xa5\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60"
        "\x82";
    return WebKit::WebData(red_square, arraysize(red_square));
  }
  return webkit_glue::WebKitPlatformSupportImpl::loadResource(name);
}

WebKit::WebString BrowserWebKitInit::defaultLocale() {
  return ASCIIToUTF16(_Context->locale());
}

WebKit::WebStorageNamespace* BrowserWebKitInit::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  return new BrowserWebStorageNamespaceImpl(DOM_STORAGE_LOCAL);
}

void BrowserWebKitInit::dispatchStorageEvent(const WebKit::WebString& key,
    const WebKit::WebString& old_value, const WebKit::WebString& new_value,
    const WebKit::WebString& origin, const WebKit::WebURL& url,
    bool is_local_storage) {
  // The event is dispatched by the proxy.
}

WebKit::WebIDBFactory* BrowserWebKitInit::idbFactory() {
  return WebKit::WebIDBFactory::create();
}

void BrowserWebKitInit::createIDBKeysFromSerializedValuesAndKeyPath(
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
    const WebKit::WebString& keyPath,
    WebKit::WebVector<WebKit::WebIDBKey>& keys_out) {
  WebKit::WebVector<WebKit::WebIDBKey> keys(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    keys[i] = WebKit::WebIDBKey::createFromValueAndKeyPath(
        values[i], WebKit::WebIDBKeyPath::create(keyPath));
  }
  keys_out.swap(keys);
}

WebKit::WebSerializedScriptValue
BrowserWebKitInit::injectIDBKeyIntoSerializedValue(
    const WebKit::WebIDBKey& key,
    const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebString& keyPath) {
  return WebKit::WebIDBKey::injectIDBKeyIntoSerializedValue(
      key, value, WebKit::WebIDBKeyPath::create(keyPath));
}

WebKit::WebGraphicsContext3D* BrowserWebKitInit::createGraphicsContext3D() {
  const CefSettings& settings = _Context->settings();
#if defined(OS_WIN)
  bool use_command_buffer =
      (settings.graphics_implementation == ANGLE_IN_PROCESS_COMMAND_BUFFER ||
       settings.graphics_implementation == DESKTOP_IN_PROCESS_COMMAND_BUFFER);
#else
  bool use_command_buffer =
      (settings.graphics_implementation == DESKTOP_IN_PROCESS_COMMAND_BUFFER);
#endif

  if (use_command_buffer) {
    return new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl();
  } else {
    return new webkit::gpu::WebGraphicsContext3DInProcessImpl(
        gfx::kNullPluginWindow, NULL);
  }
}

void BrowserWebKitInit::GetPlugins(
    bool refresh, std::vector<webkit::WebPluginInfo>* plugins) {
  if (refresh)
    webkit::npapi::PluginList::Singleton()->RefreshPlugins();
  webkit::npapi::PluginList::Singleton()->GetPlugins(plugins);
}

string16 BrowserWebKitInit::GetLocalizedString(int message_id) {
  return webkit_glue::GetLocalizedString(message_id);
}

base::StringPiece BrowserWebKitInit::GetDataResource(int resource_id) {
  return webkit_glue::GetDataResource(resource_id);
}

webkit_glue::ResourceLoaderBridge* BrowserWebKitInit::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  return BrowserResourceLoaderBridge::Create(request_info);
}

webkit_glue::WebSocketStreamHandleBridge*
BrowserWebKitInit::CreateWebSocketBridge(
    WebKit::WebSocketStreamHandle* handle,
    webkit_glue::WebSocketStreamHandleDelegate* delegate) {
  return BrowserSocketStreamBridge::Create(handle, delegate);
}

WebKit::WebString BrowserWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name) {
  switch (name) {
    case WebKit::WebLocalizedString::ValidationValueMissing:
    case WebKit::WebLocalizedString::ValidationValueMissingForCheckbox:
    case WebKit::WebLocalizedString::ValidationValueMissingForFile:
    case WebKit::WebLocalizedString::ValidationValueMissingForMultipleFile:
    case WebKit::WebLocalizedString::ValidationValueMissingForRadio:
    case WebKit::WebLocalizedString::ValidationValueMissingForSelect:
      return ASCIIToUTF16("value missing");
    case WebKit::WebLocalizedString::ValidationTypeMismatch:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForEmail:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForMultipleEmail:
    case WebKit::WebLocalizedString::ValidationTypeMismatchForURL:
      return ASCIIToUTF16("type mismatch");
    case WebKit::WebLocalizedString::ValidationPatternMismatch:
      return ASCIIToUTF16("pattern mismatch");
    case WebKit::WebLocalizedString::ValidationTooLong:
      return ASCIIToUTF16("too long");
    case WebKit::WebLocalizedString::ValidationRangeUnderflow:
      return ASCIIToUTF16("range underflow");
    case WebKit::WebLocalizedString::ValidationRangeOverflow:
      return ASCIIToUTF16("range overflow");
    case WebKit::WebLocalizedString::ValidationStepMismatch:
      return ASCIIToUTF16("step mismatch");
    default:
      return WebKitPlatformSupportImpl::queryLocalizedString(name);
  }
}

WebKit::WebString BrowserWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name, const WebKit::WebString& value) {
  if (name == WebKit::WebLocalizedString::ValidationRangeUnderflow)
    return ASCIIToUTF16("range underflow");
  if (name == WebKit::WebLocalizedString::ValidationRangeOverflow)
    return ASCIIToUTF16("range overflow");
  return WebKitPlatformSupportImpl::queryLocalizedString(name, value);
}

WebKit::WebString BrowserWebKitInit::queryLocalizedString(
    WebKit::WebLocalizedString::Name name,
    const WebKit::WebString& value1,
    const WebKit::WebString& value2) {
  if (name == WebKit::WebLocalizedString::ValidationTooLong)
    return ASCIIToUTF16("too long");
  if (name == WebKit::WebLocalizedString::ValidationStepMismatch)
    return ASCIIToUTF16("step mismatch");
  return WebKitPlatformSupportImpl::queryLocalizedString(
      name, value1, value2);
}
