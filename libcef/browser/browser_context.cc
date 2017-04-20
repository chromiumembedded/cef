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

CefBrowserContext::CefBrowserContext(bool is_proxy)
    : is_proxy_(is_proxy),
      extension_system_(NULL) {
}

CefBrowserContext::~CefBrowserContext() {
  // Should be cleared in Shutdown().
  DCHECK(!resource_context_.get());
}

void CefBrowserContext::Initialize() {
  content::BrowserContext::Initialize(this, GetPath());

  resource_context_.reset(
      new CefResourceContext(IsOffTheRecord(), GetHandler()));

  // This must be called before creating any services to avoid hitting
  // DependencyManager::AssertContextWasntDestroyed when creating/destroying
  // multiple browser contexts (due to pointer address reuse).
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  const bool extensions_enabled = extensions::ExtensionsEnabled();
  if (extensions_enabled) {
    // Create the custom ExtensionSystem first because other KeyedServices
    // depend on it.
    // The same CefExtensionSystem instance is shared by CefBrowserContextImpl
    // and CefBrowserContextProxy objects.
    extension_system_ = static_cast<extensions::CefExtensionSystem*>(
        extensions::ExtensionSystem::Get(this));
    if (is_proxy_) {
      DCHECK(extension_system_->initialized());
    } else {
      extension_system_->InitForRegularProfile(true);
    }
    resource_context_->set_extensions_info_map(extension_system_->info_map());
  }
}

void CefBrowserContext::PostInitialize() {
  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  PrefService* pref_service = GetPrefs();
  DCHECK(pref_service);
  user_prefs::UserPrefs::Set(this, pref_service);

  const bool extensions_enabled = extensions::ExtensionsEnabled();
  if (extensions_enabled && !is_proxy_)
    extension_system_->Init();
}

void CefBrowserContext::Shutdown() {
  CEF_REQUIRE_UIT();

  // Send notifications to clean up objects associated with this Profile.
  MaybeSendDestroyedNotification();

  // Remove any BrowserContextKeyedServiceFactory associations. This must be
  // called before the ProxyService owned by CefBrowserContextImpl is destroyed.
  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      this);

  if (!is_proxy_) {
    // Shuts down the storage partitions associated with this browser context.
    // This must be called before the browser context is actually destroyed
    // and before a clean-up task for its corresponding IO thread residents
    // (e.g. ResourceContext) is posted, so that the classes that hung on
    // StoragePartition can have time to do necessary cleanups on IO thread.
    ShutdownStoragePartitions();
  }

  if (resource_context_.get()) {
    // Destruction of the ResourceContext will trigger destruction of all
    // associated URLRequests.
    content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, resource_context_.release());
  }
}

content::ResourceContext* CefBrowserContext::GetResourceContext() {
  return resource_context_.get();
}

net::URLRequestContextGetter* CefBrowserContext::GetRequestContext() {
  CEF_REQUIRE_UIT();
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter* CefBrowserContext::CreateMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContext::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return nullptr;
}

ChromeZoomLevelPrefs* CefBrowserContext::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetStoragePartition(this, NULL)->GetZoomLevelDelegate());
}

void CefBrowserContext::OnRenderFrameDeleted(int render_process_id,
                                             int render_frame_id,
                                             bool is_main_frame,
                                             bool is_guest_view) {
  CEF_REQUIRE_UIT();
  if (resource_context_ && is_main_frame) {
    DCHECK_GE(render_process_id, 0);
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
        base::Bind(&CefResourceContext::ClearPluginLoadDecision,
                   base::Unretained(resource_context_.get()),
                   render_process_id));
  }
}

void CefBrowserContext::OnPurgePluginListCache() {
  CEF_REQUIRE_UIT();
  if (resource_context_) {
    // Using base::Unretained() is safe because both this callback and possible
    // deletion of |resource_context_| will execute on the IO thread, and this
    // callback will be executed first.
    CEF_POST_TASK(CEF_IOT,
        base::Bind(&CefResourceContext::ClearPluginLoadDecision,
                   base::Unretained(resource_context_.get()), -1));
  }
}
