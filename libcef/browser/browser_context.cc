// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/logging.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

#ifndef NDEBUG
base::AtomicRefCount CefBrowserContext::DebugObjCt = 0;
#endif

CefBrowserContext::CefBrowserContext()
    : extension_system_(NULL) {
#ifndef NDEBUG
  base::AtomicRefCountInc(&DebugObjCt);
#endif
}

CefBrowserContext::~CefBrowserContext() {
  // Should be cleared in Shutdown().
  DCHECK(!resource_context_.get());

#ifndef NDEBUG
  base::AtomicRefCountDec(&DebugObjCt);
#endif
}

void CefBrowserContext::Initialize() {
  const bool extensions_enabled = extensions::ExtensionsEnabled();
  if (extensions_enabled) {
    // Create the custom ExtensionSystem first because other KeyedServices
    // depend on it.
    extension_system_ = static_cast<extensions::CefExtensionSystem*>(
        extensions::ExtensionSystem::Get(this));
    extension_system_->InitForRegularProfile(true);
  }

  resource_context_.reset(new CefResourceContext(
      IsOffTheRecord(),
      extensions_enabled ? extension_system_->info_map() : NULL,
      GetHandler()));

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  PrefService* pref_service = GetPrefs();
  DCHECK(pref_service);
  user_prefs::UserPrefs::Set(this, pref_service);

  if (extensions_enabled)
    extension_system_->Init();
}

void CefBrowserContext::Shutdown() {
  CEF_REQUIRE_UIT();

  if (resource_context_.get()) {
    // Destruction of the ResourceContext will trigger destruction of all
    // associated URLRequests.
    content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, resource_context_.release());
  }

  // Remove any BrowserContextKeyedServiceFactory associations. This must be
  // called before the ProxyService owned by CefBrowserContextImpl is destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);
}

content::ResourceContext* CefBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

ChromeZoomLevelPrefs* CefBrowserContext::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetStoragePartition(this, NULL)->GetZoomLevelDelegate());
}
