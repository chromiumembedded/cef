// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PAGE_MODEL_CACHE_H_
#define CEF_LIBCEF_BROWSER_PAGE_MODEL_CACHE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cef/include/cef_values.h"

// Agent-oriented page model cache with generation-based invalidation.
// Caches snapshot results, AX tree representations, and selector mappings.
// Thread-safe via internal locking.
//
// IMPORTANT: Cache is invalidated on any DOM mutation. Callers must call
// InvalidateFrame() when the DOM changes to prevent stale data from driving
// agent actions (e.g., clicking at cached coordinates that have moved).
class CefPageModelCache {
 public:
  CefPageModelCache();
  ~CefPageModelCache();

  CefPageModelCache(const CefPageModelCache&) = delete;
  CefPageModelCache& operator=(const CefPageModelCache&) = delete;

  // DOM generation tracking. Each DOM mutation bumps the generation.
  uint64_t GetGeneration(int frame_id) const;
  void BumpGeneration(int frame_id);

  // Snapshot caching.
  struct SnapshotEntry {
    uint64_t generation = 0;
    std::string snapshot_text;
    base::TimeTicks cached_at;
  };

  bool GetCachedSnapshot(int frame_id,
                         const std::string& settings_key,
                         SnapshotEntry* out) const;
  void PutSnapshot(int frame_id,
                   const std::string& settings_key,
                   const std::string& snapshot_text);

  // Selector mapping cache. Maps CSS selectors to resolved element info.
  struct SelectorEntry {
    uint64_t generation = 0;
    bool found = false;
    int element_node_id = -1;
    std::string tag_name;
    base::TimeTicks cached_at;
  };

  bool GetCachedSelector(int frame_id,
                         const std::string& selector,
                         SelectorEntry* out) const;
  void PutSelector(int frame_id,
                   const std::string& selector,
                   bool found,
                   int element_node_id,
                   const std::string& tag_name);

  // Invalidate all cached data for a frame (call on DOM mutation).
  void InvalidateFrame(int frame_id);

  // Invalidate everything.
  void InvalidateAll();

  // Statistics.
  size_t GetTotalEntries() const;

 private:
  mutable base::Lock lock_;

  // Per-frame generation counters.
  std::map<int, uint64_t> generations_;

  // Per-frame, per-settings-key snapshot cache.
  std::map<int, std::map<std::string, SnapshotEntry>> snapshot_cache_;

  // Per-frame, per-selector cache.
  std::map<int, std::map<std::string, SelectorEntry>> selector_cache_;
};

#endif  // CEF_LIBCEF_BROWSER_PAGE_MODEL_CACHE_H_
