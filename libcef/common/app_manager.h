// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_APP_MANAGER_H_
#define CEF_LIBCEF_COMMON_APP_MANAGER_H_
#pragma once

#include <list>

#include "include/cef_app.h"
#include "include/cef_request_context.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "content/public/common/content_client.h"

class CefBrowserContext;
struct CefSchemeInfo;

// Exposes global application state in the main and render processes.
class CefAppManager {
 public:
  CefAppManager(const CefAppManager&) = delete;
  CefAppManager& operator=(const CefAppManager&) = delete;

  // Returns the singleton instance that is scoped to CEF lifespan.
  static CefAppManager* Get();

  // The following methods are available in both processes.

  virtual CefRefPtr<CefApp> GetApplication() = 0;
  virtual content::ContentClient* GetContentClient() = 0;

  // Custom scheme information will be registered first with all processes
  // (url/url_util.h) via ContentClient::AddAdditionalSchemes which calls
  // AddCustomScheme, and second with Blink (SchemeRegistry) via
  // ContentRendererClient::WebKitInitialized which calls GetCustomSchemes.
  void AddCustomScheme(const CefSchemeInfo* scheme_info);
  bool HasCustomScheme(const std::string& scheme_name);

  using SchemeInfoList = std::list<CefSchemeInfo>;
  const SchemeInfoList* GetCustomSchemes();

  // Called from ContentClient::AddAdditionalSchemes.
  void AddAdditionalSchemes(content::ContentClient::Schemes* schemes);

  // The following methods are only available in the main (browser) process.

  // Called from CefRequestContextImpl. |initialized_cb| may be executed
  // synchronously or asynchronously.
  virtual CefRefPtr<CefRequestContext> GetGlobalRequestContext() = 0;
  virtual CefBrowserContext* CreateNewBrowserContext(
      const CefRequestContextSettings& settings,
      base::OnceClosure initialized_cb) = 0;

#if BUILDFLAG(IS_WIN)
  // Returns the module name (usually libcef.dll).
  const wchar_t* GetResourceDllName();
#endif

 protected:
  CefAppManager();
  virtual ~CefAppManager();

 private:
  // Custom schemes handled by the client.
  SchemeInfoList scheme_info_list_;
  bool scheme_info_list_locked_ = false;
};

#endif  // CEF_LIBCEF_COMMON_APP_MANAGER_H_
