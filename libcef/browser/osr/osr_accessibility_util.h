// Copyright 2017 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#define CEF_LIBCEF_BROWSER_OSR_ACCESSIBILITY_UTIL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "cef/include/cef_values.h"

namespace ui {
struct AXLocationAndScrollUpdates;
struct AXUpdatesAndEvents;
class AXTreeID;
}  // namespace ui

namespace osr_accessibility_util {

// Convert Accessibility Event and location updates to CefValue, which may be
// consumed or serialized with CefJSONWrite.
CefRefPtr<CefValue> ParseAccessibilityEventData(
    const ui::AXUpdatesAndEvents& details);

CefRefPtr<CefValue> ParseAccessibilityLocationData(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationAndScrollUpdates& details);

// Cache for AX-tree transform results. Invalidated by DOM generation counter.
class AXTreeTransformCache {
 public:
  AXTreeTransformCache();
  ~AXTreeTransformCache();

  // Get cached transform result, or nullptr if cache miss.
  CefRefPtr<CefValue> GetCachedEventData(
      const ui::AXTreeID& tree_id,
      uint64_t generation) const;

  // Store transform result with generation counter.
  void PutEventData(const ui::AXTreeID& tree_id,
                    uint64_t generation,
                    CefRefPtr<CefValue> value);

  // Get cached location data, or nullptr if cache miss.
  CefRefPtr<CefValue> GetCachedLocationData(
      const ui::AXTreeID& tree_id,
      uint64_t generation) const;

  void PutLocationData(const ui::AXTreeID& tree_id,
                       uint64_t generation,
                       CefRefPtr<CefValue> value);

  // Invalidate all cached data for a tree.
  void InvalidateTree(const ui::AXTreeID& tree_id);

  // Invalidate everything.
  void InvalidateAll();

  // Get cache statistics.
  size_t GetHitCount() const { return hit_count_; }
  size_t GetMissCount() const { return miss_count_; }

 private:
  struct CacheEntry {
    uint64_t generation = 0;
    CefRefPtr<CefValue> value;
  };

  // Keyed by tree_id string representation.
  std::map<std::string, CacheEntry> event_cache_;
  std::map<std::string, CacheEntry> location_cache_;
  mutable size_t hit_count_ = 0;
  mutable size_t miss_count_ = 0;
};

// Cached versions of the parse functions. Check the cache first, and on miss,
// call the original parse function and store the result.
CefRefPtr<CefValue> ParseAccessibilityEventDataCached(
    const ui::AXUpdatesAndEvents& details,
    uint64_t generation,
    AXTreeTransformCache& cache);

CefRefPtr<CefValue> ParseAccessibilityLocationDataCached(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationAndScrollUpdates& details,
    uint64_t generation,
    AXTreeTransformCache& cache);

}  // namespace osr_accessibility_util

#endif  // CEF_LIBCEF_BROWSER_ACCESSIBILITY_UTIL_H_
