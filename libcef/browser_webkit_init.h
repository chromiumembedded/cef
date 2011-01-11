// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEBKIT_INIT_H
#define _BROWSER_WEBKIT_INIT_H

#include "browser_appcache_system.h"
#include "browser_database_system.h"
#include "browser_file_system.h"
#include "browser_resource_loader_bridge.h"
#include "browser_webblobregistry_impl.h"
#include "browser_webcookiejar_impl.h"
#include "browser_webstoragenamespace_impl.h"

#include "base/file_util.h"
#include "base/metrics/stats_counters.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "media/base/media.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageEventDispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKey.h"
#include "third_party/WebKit/WebKit/chromium/public/WebIDBKeyPath.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"


class BrowserWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  explicit BrowserWebKitInit() {
    v8::V8::SetCounterFunction(base::StatsTable::FindLocation);

    WebKit::initialize(this);
    WebKit::setLayoutTestMode(false);
    WebKit::WebScriptController::registerExtension(
        extensions_v8::GearsExtension::Get());
    WebKit::WebRuntimeFeatures::enableSockets(true);
    WebKit::WebRuntimeFeatures::enableApplicationCache(true);
    WebKit::WebRuntimeFeatures::enableDatabase(true);
    WebKit::WebRuntimeFeatures::enableWebGL(true);
    WebKit::WebRuntimeFeatures::enablePushState(true);
    WebKit::WebRuntimeFeatures::enableNotifications(true);
    WebKit::WebRuntimeFeatures::enableTouch(true);
    WebKit::WebRuntimeFeatures::enableIndexedDatabase(true);
    WebKit::WebRuntimeFeatures::enableGeolocation(false);
    WebKit::WebRuntimeFeatures::enableSpeechInput(true);
    WebKit::WebRuntimeFeatures::enableFileSystem(true);

    // TODO(hwennborg): Enable this once the implementation supports it.
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
 
    file_utilities_.set_sandbox_enabled(false);
  }

  ~BrowserWebKitInit() {
    WebKit::shutdown();
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }

  WebKit::WebClipboard* clipboard() {
    return &clipboard_;
  }

  virtual WebKit::WebFileUtilities* fileUtilities() {
    return &file_utilities_;
  }

  virtual WebKit::WebSandboxSupport* sandboxSupport() {
    return NULL;
  }

  virtual WebKit::WebBlobRegistry* blobRegistry() {
    return blob_registry_.get();
  }

  virtual WebKit::WebCookieJar* cookieJar() {
    return &cookie_jar_;
  }

  virtual WebKit::WebFileSystem* fileSystem() {
    return &file_system_;
  }
  
  virtual bool sandboxEnabled() {
    return true;
  }

  virtual WebKit::WebKitClient::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags) {
    return BrowserDatabaseSystem::GetInstance()->OpenFile(
        vfs_file_name, desired_flags);
  }

  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir) {
    return BrowserDatabaseSystem::GetInstance()->DeleteFile(
        vfs_file_name, sync_dir);
  }

  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name) {
    return BrowserDatabaseSystem::GetInstance()->GetFileAttributes(
        vfs_file_name);
  }

  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name) {
    return BrowserDatabaseSystem::GetInstance()->GetFileSize(vfs_file_name);
  }

  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length) {
    return 0;
  }

  virtual bool isLinkVisited(unsigned long long linkHash) {
    return false;
  }

  virtual WebKit::WebMessagePortChannel* createMessagePortChannel() {
    return NULL;
  }

  virtual void prefetchHostName(const WebKit::WebString&) {
  }

  virtual WebKit::WebData loadResource(const char* name) {
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
    return webkit_glue::WebKitClientImpl::loadResource(name);
  }

  virtual WebKit::WebString defaultLocale() {
    return ASCIIToUTF16("en-US");
  }

  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota) {
    if (BrowserWebStorageNamespaceImpl::IsStorageActive()) {
      // Use the localStorage implementation that writes data to disk.
      return new BrowserWebStorageNamespaceImpl(DOM_STORAGE_LOCAL);
    }

    // Use the default localStorage implementation.
    return WebKit::WebStorageNamespace::createLocalStorageNamespace(path,
        WebKit::WebStorageNamespace::m_localStorageQuota);
  }

  void dispatchStorageEvent(const WebKit::WebString& key,
      const WebKit::WebString& old_value, const WebKit::WebString& new_value,
      const WebKit::WebString& origin, const WebKit::WebURL& url,
      bool is_local_storage) {
    // The event is dispatched by the proxy.
  }

  virtual WebKit::WebIDBFactory* idbFactory() {
    return WebKit::WebIDBFactory::create();
  }

  virtual void createIDBKeysFromSerializedValuesAndKeyPath(
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

  virtual WebKit::WebGraphicsContext3D* createGraphicsContext3D() {
    return new webkit_gpu::WebGraphicsContext3DInProcessImpl();
  }

  WebKit::WebString queryLocalizedString(
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
        return WebKitClientImpl::queryLocalizedString(name);
    }
  }
  
  WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name, const WebKit::WebString& value) {
    if (name == WebKit::WebLocalizedString::ValidationRangeUnderflow)
      return ASCIIToUTF16("range underflow");
    if (name == WebKit::WebLocalizedString::ValidationRangeOverflow)
      return ASCIIToUTF16("range overflow");
    return WebKitClientImpl::queryLocalizedString(name, value);
  }
  
  WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1,
      const WebKit::WebString& value2) {
    if (name == WebKit::WebLocalizedString::ValidationTooLong)
      return ASCIIToUTF16("too long");
    if (name == WebKit::WebLocalizedString::ValidationStepMismatch)
      return ASCIIToUTF16("step mismatch");
    return WebKitClientImpl::queryLocalizedString(name, value1, value2);
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  webkit_glue::WebClipboardImpl clipboard_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  ScopedTempDir appcache_dir_;
  BrowserAppCacheSystem appcache_system_;
  BrowserDatabaseSystem database_system_;
  BrowserWebCookieJarImpl cookie_jar_;
  scoped_refptr<BrowserWebBlobRegistryImpl> blob_registry_;
  BrowserFileSystem file_system_;
};

#endif  // _BROWSER_WEBKIT_INIT_H
