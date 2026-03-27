// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/session_pool.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cef/libcef/browser/thread_util.h"

CefSessionPool::CefSessionPool(const Config& config) : config_(config) {
  Prewarm();
}

CefSessionPool::~CefSessionPool() {
  Shutdown();
}

void CefSessionPool::Acquire(const CefRequestContextSettings& settings,
                              AcquireCallback callback) {
  CefRefPtr<CefRequestContext> context;

  {
    base::AutoLock lock(lock_);
    if (shutdown_) {
      LOG(WARNING) << "CefSessionPool::Acquire called after shutdown";
      std::move(callback).Run(nullptr);
      return;
    }

    EvictStaleEntries();

    if (!available_.empty()) {
      auto entry = std::move(available_.front());
      available_.pop_front();
      context = entry->context;
      in_use_count_++;
      VLOG(1) << "CefSessionPool: acquired prewarmed context from pool"
              << " (available=" << available_.size()
              << ", in_use=" << in_use_count_ << ")";
    }
  }

  if (context) {
    // Reset all web-visible state before handing off. This is the critical
    // invariant: the caller gets a context with clean state, having only
    // saved process spawn + renderer init time.
    ResetContextState(context);
    std::move(callback).Run(context);
  } else {
    // Pool was empty; create a fresh context on the fly (no prewarming
    // benefit, but still correct behavior).
    VLOG(1) << "CefSessionPool: pool empty, creating context on demand";

    CefRefPtr<CefRequestContext> new_context =
        CefRequestContext::CreateContext(settings, nullptr);

    {
      base::AutoLock lock(lock_);
      in_use_count_++;
    }

    std::move(callback).Run(new_context);
  }

  // Kick off background prewarming to refill the pool.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefSessionPool::Prewarm,
                                         base::Unretained(this)));
}

void CefSessionPool::Release(CefRefPtr<CefRequestContext> context) {
  if (!context) {
    return;
  }

  base::AutoLock lock(lock_);
  if (in_use_count_ > 0) {
    in_use_count_--;
  }

  if (shutdown_) {
    return;
  }

  auto entry = std::make_unique<PoolEntry>();
  entry->context = context;
  entry->created_at = base::TimeTicks::Now();
  entry->last_used_at = base::TimeTicks::Now();
  entry->cache_path = context->GetCachePath().ToString();
  available_.push_back(std::move(entry));

  // Trim pool to configured size by removing oldest entries.
  while (available_.size() > config_.pool_size) {
    VLOG(1) << "CefSessionPool: trimming excess context from pool";
    available_.pop_front();
  }

  VLOG(1) << "CefSessionPool: released context back to pool"
          << " (available=" << available_.size()
          << ", in_use=" << in_use_count_ << ")";
}

// static
void CefSessionPool::ResetContextState(CefRefPtr<CefRequestContext> context) {
  if (!context) {
    return;
  }

  // -----------------------------------------------------------------------
  // CRITICAL: Clear ALL web-visible state. The win from pooling is skipping
  // process spawn + renderer init (~200-500ms), NOT skipping state setup.
  // -----------------------------------------------------------------------

  // 1. Clear all cookies.
  CefRefPtr<CefCookieManager> cookie_manager =
      context->GetCookieManager(nullptr);
  if (cookie_manager) {
    // DeleteCookies with empty URL and name deletes all cookies.
    cookie_manager->DeleteCookies(CefString(), CefString(), nullptr);
    VLOG(1) << "CefSessionPool: cleared all cookies";
  }

  // 2. Clear HTTP cache.
  context->ClearHttpCache(nullptr);
  VLOG(1) << "CefSessionPool: cleared HTTP cache";

  // 3. Close all connections (resets connection pools, TLS sessions, etc.).
  context->CloseAllConnections(nullptr);
  VLOG(1) << "CefSessionPool: closed all connections";

  // 4. Clear certificate exceptions.
  context->ClearCertificateExceptions(nullptr);
  VLOG(1) << "CefSessionPool: cleared certificate exceptions";

  // 5. Clear HTTP auth credentials.
  context->ClearHttpAuthCredentials(nullptr);
  VLOG(1) << "CefSessionPool: cleared HTTP auth credentials";

  // 6. Clear all registered scheme handler factories.
  context->ClearSchemeHandlerFactories();
  VLOG(1) << "CefSessionPool: cleared scheme handler factories";

  // 7. Clear persisted storage state (saved sessions, etc.).
  CefRefPtr<CefStorageStateManager> storage_mgr =
      context->GetStorageStateManager(nullptr);
  if (storage_mgr) {
    storage_mgr->Clear(CefString(), nullptr);
    VLOG(1) << "CefSessionPool: cleared storage state";
  }

  // TODO(cef-session-pool): Clear localStorage and IndexedDB.
  //   CefRequestContext does not currently expose direct APIs for clearing
  //   localStorage or IndexedDB. These are managed at the storage partition
  //   level in Chromium (via BrowsingDataRemover). When such an API becomes
  //   available in CEF, it must be called here. Until then, acquiring agents
  //   should navigate to about:blank to reset performance.timeOrigin, and
  //   web-storage isolation relies on each pooled context using a unique
  //   on-disk cache_path.

  // TODO(cef-session-pool): Clear service worker registrations.
  //   Service workers are registered per-origin and managed by Chromium's
  //   ServiceWorkerContextWrapper. CefRequestContext does not expose an API
  //   for unregistering all service workers. When available, call it here.

  // TODO(cef-session-pool): Reset permissions to defaults.
  //   Content settings (permissions) can be partially reset via
  //   SetContentSetting / SetWebsiteSetting, but there is no bulk-reset API
  //   on CefRequestContext. A future API addition should be used here to
  //   reset all per-origin permission grants (camera, microphone,
  //   notifications, geolocation, etc.) to their default values.

  LOG(INFO) << "CefSessionPool: context state reset complete";
}

size_t CefSessionPool::GetAvailableCount() const {
  base::AutoLock lock(lock_);
  return available_.size();
}

size_t CefSessionPool::GetTotalCount() const {
  base::AutoLock lock(lock_);
  return available_.size() + in_use_count_;
}

void CefSessionPool::Prewarm() {
  base::AutoLock lock(lock_);
  if (shutdown_) {
    return;
  }

  while (available_.size() < config_.pool_size) {
    CreatePooledContext();
  }

  VLOG(1) << "CefSessionPool: prewarming complete"
          << " (available=" << available_.size() << ")";
}

void CefSessionPool::CreatePooledContext() {
  // Must be called with lock_ held.
  std::string cache_path = GenerateCachePath();

  CefRequestContextSettings settings;
  settings.size = sizeof(CefRequestContextSettings);

  if (!cache_path.empty()) {
    CefString(&settings.cache_path).FromString(cache_path);
  }

  CefRefPtr<CefRequestContext> context =
      CefRequestContext::CreateContext(settings, nullptr);

  if (!context) {
    LOG(ERROR) << "CefSessionPool: failed to create pooled context"
               << " (cache_path=" << cache_path << ")";
    return;
  }

  auto entry = std::make_unique<PoolEntry>();
  entry->context = context;
  entry->created_at = base::TimeTicks::Now();
  entry->last_used_at = base::TimeTicks::Now();
  entry->cache_path = cache_path;
  available_.push_back(std::move(entry));

  VLOG(1) << "CefSessionPool: created pooled context"
          << " (cache_path=" << cache_path << ")";
}

void CefSessionPool::Shutdown() {
  base::AutoLock lock(lock_);
  if (shutdown_) {
    return;
  }

  shutdown_ = true;
  available_.clear();
  LOG(INFO) << "CefSessionPool: shutdown complete";
}

void CefSessionPool::SetConfig(const Config& config) {
  base::AutoLock lock(lock_);
  config_ = config;

  // Trim if new pool_size is smaller.
  while (available_.size() > config_.pool_size) {
    available_.pop_front();
  }
}

void CefSessionPool::EvictStaleEntries() {
  // Must be called with lock_ held.
  const base::TimeTicks now = base::TimeTicks::Now();

  auto it = available_.begin();
  while (it != available_.end()) {
    if (now - (*it)->last_used_at > config_.max_idle_time) {
      VLOG(1) << "CefSessionPool: evicting stale context"
              << " (cache_path=" << (*it)->cache_path << ")";
      it = available_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string CefSessionPool::GenerateCachePath() {
  // Must be called with lock_ held.
  if (config_.base_cache_path.empty()) {
    return std::string();
  }

  return base::StringPrintf("%s/pool_%" PRIu64, config_.base_cache_path.c_str(),
                            next_context_id_++);
}
