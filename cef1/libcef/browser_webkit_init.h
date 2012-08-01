// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEBKIT_INIT_H_
#define CEF_LIBCEF_BROWSER_WEBKIT_INIT_H_
#pragma once

#include <vector>

#include "libcef/browser_appcache_system.h"
#include "libcef/browser_database_system.h"
#include "libcef/browser_dom_storage_system.h"
#include "libcef/browser_file_system.h"
#include "libcef/browser_webblobregistry_impl.h"
#include "libcef/browser_webcookiejar_impl.h"
#include "libcef/simple_clipboard_impl.h"

#include "base/scoped_temp_dir.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebGraphicsContext3D.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkitplatformsupport_impl.h"

class BrowserPrerenderingSupport;

class BrowserWebKitInit : public webkit_glue::WebKitPlatformSupportImpl {
 public:
  BrowserWebKitInit();
  virtual ~BrowserWebKitInit();

  virtual WebKit::WebMimeRegistry* mimeRegistry() OVERRIDE;
  virtual WebKit::WebClipboard* clipboard() OVERRIDE;
  virtual WebKit::WebFileUtilities* fileUtilities() OVERRIDE;
  virtual WebKit::WebSandboxSupport* sandboxSupport() OVERRIDE;
  virtual WebKit::WebBlobRegistry* blobRegistry() OVERRIDE;
  virtual WebKit::WebCookieJar* cookieJar() OVERRIDE;
  virtual WebKit::WebFileSystem* fileSystem() OVERRIDE;
  virtual bool sandboxEnabled() OVERRIDE;
  virtual WebKit::WebKitPlatformSupport::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags) OVERRIDE;
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir) OVERRIDE;
  virtual long databaseGetFileAttributes(  // NOLINT(runtime/int)
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetFileSize(  // NOLINT(runtime/int)
      const WebKit::WebString& vfs_file_name) OVERRIDE;
  virtual long long databaseGetSpaceAvailableForOrigin(  // NOLINT(runtime/int)
      const WebKit::WebString& origin_identifier) OVERRIDE;
  virtual unsigned long long visitedLinkHash(  // NOLINT(runtime/int)
      const char* canonicalURL,
      size_t length) OVERRIDE;
  virtual bool isLinkVisited(
      unsigned long long linkHash) OVERRIDE;  // NOLINT(runtime/int)
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel() OVERRIDE;
  virtual void prefetchHostName(const WebKit::WebString&) OVERRIDE;
  virtual void decrementStatsCounter(const char* name) OVERRIDE;
  virtual void incrementStatsCounter(const char* name) OVERRIDE;
  virtual void histogramCustomCounts(const char* name, int sample, int min,
                                     int max, int bucket_count) OVERRIDE;
  virtual void histogramEnumeration(const char* name, int sample,
                                    int boundary_value) OVERRIDE;
  virtual WebKit::WebData loadResource(const char* name) OVERRIDE;
  virtual WebKit::WebString defaultLocale() OVERRIDE;
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota) OVERRIDE;
  virtual WebKit::WebIDBFactory* idbFactory() OVERRIDE;
  virtual void createIDBKeysFromSerializedValuesAndKeyPath(
      const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
      const WebKit::WebIDBKeyPath& keyPath,
      WebKit::WebVector<WebKit::WebIDBKey>& keys_out) OVERRIDE;
  virtual WebKit::WebSerializedScriptValue injectIDBKeyIntoSerializedValue(
      const WebKit::WebIDBKey& key,
      const WebKit::WebSerializedScriptValue& value,
      const WebKit::WebIDBKeyPath& keyPath) OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* createOffscreenGraphicsContext3D(
      const WebKit::WebGraphicsContext3D::Attributes& attributes) OVERRIDE;
  virtual string16 GetLocalizedString(int message_id) OVERRIDE;
  virtual base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) OVERRIDE;
  virtual void GetPlugins(bool refresh,
                          std::vector<webkit::WebPluginInfo>* plugins) OVERRIDE;
  virtual webkit_glue::ResourceLoaderBridge* CreateResourceLoader(
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
      OVERRIDE;
  virtual webkit_glue::WebSocketStreamHandleBridge* CreateWebSocketBridge(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name) OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name, const WebKit::WebString& value)
      OVERRIDE;
  virtual WebKit::WebString queryLocalizedString(
      WebKit::WebLocalizedString::Name name,
      const WebKit::WebString& value1,
      const WebKit::WebString& value2) OVERRIDE;

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  webkit_glue::WebClipboardImpl clipboard_;
  SimpleClipboardClient clipboard_client_;
  webkit_glue::WebFileUtilitiesImpl file_utilities_;
  BrowserAppCacheSystem appcache_system_;
  BrowserDatabaseSystem database_system_;
  BrowserDomStorageSystem dom_storage_system_;
  BrowserWebCookieJarImpl cookie_jar_;
  scoped_refptr<BrowserWebBlobRegistryImpl> blob_registry_;
  scoped_ptr<BrowserPrerenderingSupport> prerendering_support_;
};

#endif  // CEF_LIBCEF_BROWSER_WEBKIT_INIT_H_
