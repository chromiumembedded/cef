// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEBKIT_INIT_H
#define _BROWSER_WEBKIT_INIT_H

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "media/base/media.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/extensions/v8/interval_extension.h"
#include "webkit/api/public/WebCString.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebStorageNamespace.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "browser_appcache_system.h"
#include "browser_database_system.h"
#include "browser_resource_loader_bridge.h"


class BrowserWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  explicit BrowserWebKitInit() {
    v8::V8::SetCounterFunction(StatsTable::FindLocation);

    WebKit::initialize(this);
    WebKit::setLayoutTestMode(false);
    WebKit::registerURLSchemeAsLocal(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
    WebKit::registerURLSchemeAsNoAccess(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
    WebKit::registerExtension(extensions_v8::GearsExtension::Get());
    WebKit::registerExtension(extensions_v8::IntervalExtension::Get());

    // Load libraries for media and enable the media player.
    FilePath module_path;
    if (PathService::Get(base::DIR_MODULE, &module_path) &&
        media::InitializeMediaLibrary(module_path)) {
      WebKit::enableMediaPlayer();
    }

    // Construct and initialize an appcache system for this scope.
    // A new empty temp directory is created to house any cached
    // content during the run. Upon exit that directory is deleted.
    if (appcache_dir_.CreateUniqueTempDir())
      BrowserAppCacheSystem::InitializeOnUIThread(appcache_dir_.path());
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

  virtual WebKit::WebSandboxSupport* sandboxSupport() {
    return NULL;
  }

  virtual bool sandboxEnabled() {
    return true;
  }

  virtual WebKit::WebKitClient::FileHandle databaseOpenFile(
      const WebKit::WebString& file_name, int desired_flags,
      WebKit::WebKitClient::FileHandle* dir_handle) {
    return BrowserDatabaseSystem::GetInstance()->OpenFile(
        webkit_glue::WebStringToFilePath(file_name),
        desired_flags, dir_handle);
  }

  virtual int databaseDeleteFile(const WebKit::WebString& file_name,
                                 bool sync_dir) {
    return BrowserDatabaseSystem::GetInstance()->DeleteFile(
        webkit_glue::WebStringToFilePath(file_name), sync_dir);
  }

  virtual long databaseGetFileAttributes(const WebKit::WebString& file_name) {
    return BrowserDatabaseSystem::GetInstance()->GetFileAttributes(
        webkit_glue::WebStringToFilePath(file_name));
  }

  virtual long long databaseGetFileSize(const WebKit::WebString& file_name) {
    return BrowserDatabaseSystem::GetInstance()->GetFileSize(
        webkit_glue::WebStringToFilePath(file_name));
  }

  virtual bool getFileSize(const WebKit::WebString& path, long long& result) {
    return file_util::GetFileSize(
        webkit_glue::WebStringToFilePath(path),
        reinterpret_cast<int64*>(&result));
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

  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString& value) {
    BrowserResourceLoaderBridge::SetCookie(
        url, first_party_for_cookies, value.utf8());
  }

  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url,
      const WebKit::WebURL& first_party_for_cookies) {
    return WebKit::WebString::fromUTF8(BrowserResourceLoaderBridge::GetCookies(
        url, first_party_for_cookies));
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
      const WebKit::WebString& path) {
    return WebKit::WebStorageNamespace::createLocalStorageNamespace(path);
  }

  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace() {
    return WebKit::WebStorageNamespace::createSessionStorageNamespace();
  }

  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
        WebKit::WebApplicationCacheHostClient* client) {
    return BrowserAppCacheSystem::CreateApplicationCacheHost(client);
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
  webkit_glue::WebClipboardImpl clipboard_;
  ScopedTempDir appcache_dir_;
  BrowserAppCacheSystem appcache_system_;
  BrowserDatabaseSystem database_system_;
};

#endif  // _BROWSER_WEBKIT_INIT_H
