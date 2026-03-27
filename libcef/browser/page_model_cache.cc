// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/page_model_cache.h"

#include "base/auto_reset.h"

CefPageModelCache::CefPageModelCache() = default;
CefPageModelCache::~CefPageModelCache() = default;

uint64_t CefPageModelCache::GetGeneration(int frame_id) const {
  base::AutoLock lock(lock_);
  auto it = generations_.find(frame_id);
  if (it != generations_.end()) {
    return it->second;
  }
  return 0;
}

void CefPageModelCache::BumpGeneration(int frame_id) {
  base::AutoLock lock(lock_);
  ++generations_[frame_id];
}

bool CefPageModelCache::GetCachedSnapshot(int frame_id,
                                          const std::string& settings_key,
                                          SnapshotEntry* out) const {
  base::AutoLock lock(lock_);

  auto frame_it = snapshot_cache_.find(frame_id);
  if (frame_it == snapshot_cache_.end()) {
    return false;
  }

  auto entry_it = frame_it->second.find(settings_key);
  if (entry_it == frame_it->second.end()) {
    return false;
  }

  // Check generation-based validity.
  auto gen_it = generations_.find(frame_id);
  uint64_t current_gen = (gen_it != generations_.end()) ? gen_it->second : 0;
  if (entry_it->second.generation != current_gen) {
    return false;
  }

  if (out) {
    *out = entry_it->second;
  }
  return true;
}

void CefPageModelCache::PutSnapshot(int frame_id,
                                    const std::string& settings_key,
                                    const std::string& snapshot_text) {
  base::AutoLock lock(lock_);

  uint64_t current_gen = generations_[frame_id];

  SnapshotEntry entry;
  entry.generation = current_gen;
  entry.snapshot_text = snapshot_text;
  entry.cached_at = base::TimeTicks::Now();

  snapshot_cache_[frame_id][settings_key] = std::move(entry);
}

bool CefPageModelCache::GetCachedSelector(int frame_id,
                                          const std::string& selector,
                                          SelectorEntry* out) const {
  base::AutoLock lock(lock_);

  auto frame_it = selector_cache_.find(frame_id);
  if (frame_it == selector_cache_.end()) {
    return false;
  }

  auto entry_it = frame_it->second.find(selector);
  if (entry_it == frame_it->second.end()) {
    return false;
  }

  // Check generation-based validity.
  auto gen_it = generations_.find(frame_id);
  uint64_t current_gen = (gen_it != generations_.end()) ? gen_it->second : 0;
  if (entry_it->second.generation != current_gen) {
    return false;
  }

  if (out) {
    *out = entry_it->second;
  }
  return true;
}

void CefPageModelCache::PutSelector(int frame_id,
                                    const std::string& selector,
                                    bool found,
                                    int element_node_id,
                                    const std::string& tag_name) {
  base::AutoLock lock(lock_);

  uint64_t current_gen = generations_[frame_id];

  SelectorEntry entry;
  entry.generation = current_gen;
  entry.found = found;
  entry.element_node_id = element_node_id;
  entry.tag_name = tag_name;
  entry.cached_at = base::TimeTicks::Now();

  selector_cache_[frame_id][selector] = std::move(entry);
}

void CefPageModelCache::InvalidateFrame(int frame_id) {
  base::AutoLock lock(lock_);
  ++generations_[frame_id];
  snapshot_cache_.erase(frame_id);
  selector_cache_.erase(frame_id);
}

void CefPageModelCache::InvalidateAll() {
  base::AutoLock lock(lock_);
  generations_.clear();
  snapshot_cache_.clear();
  selector_cache_.clear();
}

size_t CefPageModelCache::GetTotalEntries() const {
  base::AutoLock lock(lock_);

  size_t total = 0;
  for (const auto& frame_pair : snapshot_cache_) {
    total += frame_pair.second.size();
  }
  for (const auto& frame_pair : selector_cache_) {
    total += frame_pair.second.size();
  }
  return total;
}
