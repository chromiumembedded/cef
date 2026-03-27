// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SESSION_POOL_H_
#define CEF_LIBCEF_BROWSER_SESSION_POOL_H_
#pragma once

#include <deque>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cef/include/cef_request_context.h"

// Session pool for prewarmed browser contexts.
//
// Maintains a pool of pre-initialized CefRequestContext instances that have
// completed the expensive process spawn and renderer initialization. When an
// agent needs a new session, it acquires a prewarmed context from the pool
// instead of creating one from scratch, saving ~200-500ms of startup time.
//
// IMPORTANT: On acquisition, ALL web-visible state is reset:
//   - Cookies are cleared
//   - localStorage / IndexedDB are cleared
//   - HTTP cache is cleared
//   - Service worker registrations are removed
//   - Permissions are reset to defaults
//   - performance.timeOrigin is effectively reset (new navigation)
//
// The pool only saves process/renderer infrastructure initialization time.
// Auth state must be replayed through actual auth flows, never pre-injected.
//
// Thread-safe via internal locking.
class CefSessionPool {
 public:
  struct Config {
    // Number of contexts to keep prewarmed in the pool.
    size_t pool_size = 2;

    // Maximum idle time before a prewarmed context is discarded.
    base::TimeDelta max_idle_time = base::Minutes(5);

    // Base cache path for pooled contexts (each gets a unique subdirectory).
    std::string base_cache_path;
  };

  explicit CefSessionPool(const Config& config = Config());
  ~CefSessionPool();

  CefSessionPool(const CefSessionPool&) = delete;
  CefSessionPool& operator=(const CefSessionPool&) = delete;

  // Acquire a prewarmed context from the pool.
  // If the pool is empty, creates a new context (no prewarming benefit).
  // The returned context has all web-visible state cleared.
  // |settings| are applied after state reset.
  using AcquireCallback =
      base::OnceCallback<void(CefRefPtr<CefRequestContext>)>;
  void Acquire(const CefRequestContextSettings& settings,
               AcquireCallback callback);

  // Release a context back to the pool for reuse.
  // State will be cleared before the context is made available again.
  void Release(CefRefPtr<CefRequestContext> context);

  // Get the number of available (prewarmed) contexts in the pool.
  size_t GetAvailableCount() const;

  // Get the total number of contexts managed by the pool (available + in use).
  size_t GetTotalCount() const;

  // Start prewarming contexts to fill the pool.
  void Prewarm();

  // Shut down the pool and release all contexts.
  void Shutdown();

  // Update pool configuration.
  void SetConfig(const Config& config);

 private:
  struct PoolEntry {
    CefRefPtr<CefRequestContext> context;
    base::TimeTicks created_at;
    base::TimeTicks last_used_at;
    std::string cache_path;
  };

  // Create a new context for the pool.
  void CreatePooledContext();

  // Clear all web-visible state from a context.
  static void ResetContextState(CefRefPtr<CefRequestContext> context);

  // Evict idle contexts that have exceeded max_idle_time.
  void EvictStaleEntries();

  // Generate a unique cache path for a pooled context.
  std::string GenerateCachePath();

  mutable base::Lock lock_;
  Config config_;
  bool shutdown_ = false;
  uint64_t next_context_id_ = 1;

  // Available (prewarmed, ready-to-use) contexts.
  std::deque<std::unique_ptr<PoolEntry>> available_;

  // Count of contexts currently in use (acquired but not released).
  size_t in_use_count_ = 0;
};

#endif  // CEF_LIBCEF_BROWSER_SESSION_POOL_H_
