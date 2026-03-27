// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/page_model_cache.h"
#include "tests/gtest/include/gtest/gtest.h"

TEST(PageModelCache, InitialGeneration) {
  CefPageModelCache cache;
  // A frame that has never been bumped should have generation 0.
  EXPECT_EQ(0u, cache.GetGeneration(/*frame_id=*/1));
}

TEST(PageModelCache, BumpGeneration) {
  CefPageModelCache cache;
  const int frame_id = 1;

  EXPECT_EQ(0u, cache.GetGeneration(frame_id));
  cache.BumpGeneration(frame_id);
  EXPECT_EQ(1u, cache.GetGeneration(frame_id));
  cache.BumpGeneration(frame_id);
  EXPECT_EQ(2u, cache.GetGeneration(frame_id));
}

TEST(PageModelCache, SnapshotCacheMiss) {
  CefPageModelCache cache;
  CefPageModelCache::SnapshotEntry entry;

  // Getting a snapshot that was never put should return false.
  EXPECT_FALSE(cache.GetCachedSnapshot(/*frame_id=*/1, "settings_a", &entry));
}

TEST(PageModelCache, SnapshotCacheHit) {
  CefPageModelCache cache;
  const int frame_id = 1;
  const std::string settings_key = "settings_a";
  const std::string snapshot_text = "<html>snapshot content</html>";

  cache.PutSnapshot(frame_id, settings_key, snapshot_text);

  CefPageModelCache::SnapshotEntry entry;
  EXPECT_TRUE(cache.GetCachedSnapshot(frame_id, settings_key, &entry));
  EXPECT_EQ(snapshot_text, entry.snapshot_text);
  EXPECT_EQ(0u, entry.generation);
}

TEST(PageModelCache, SnapshotInvalidatedByBump) {
  CefPageModelCache cache;
  const int frame_id = 1;

  cache.PutSnapshot(frame_id, "key", "data");

  CefPageModelCache::SnapshotEntry entry;
  EXPECT_TRUE(cache.GetCachedSnapshot(frame_id, "key", &entry));

  // Bump generation to simulate a DOM mutation.
  cache.BumpGeneration(frame_id);

  // The cached snapshot should now be stale (generation mismatch).
  EXPECT_FALSE(cache.GetCachedSnapshot(frame_id, "key", &entry));
}

TEST(PageModelCache, SelectorCacheHit) {
  CefPageModelCache cache;
  const int frame_id = 1;
  const std::string selector = "#login-button";

  cache.PutSelector(frame_id, selector, /*found=*/true,
                    /*element_node_id=*/42, /*tag_name=*/"button");

  CefPageModelCache::SelectorEntry entry;
  EXPECT_TRUE(cache.GetCachedSelector(frame_id, selector, &entry));
  EXPECT_TRUE(entry.found);
  EXPECT_EQ(42, entry.element_node_id);
  EXPECT_EQ("button", entry.tag_name);
}

TEST(PageModelCache, SelectorInvalidatedByBump) {
  CefPageModelCache cache;
  const int frame_id = 1;

  cache.PutSelector(frame_id, ".cls", true, 10, "div");

  CefPageModelCache::SelectorEntry entry;
  EXPECT_TRUE(cache.GetCachedSelector(frame_id, ".cls", &entry));

  cache.BumpGeneration(frame_id);
  EXPECT_FALSE(cache.GetCachedSelector(frame_id, ".cls", &entry));
}

TEST(PageModelCache, InvalidateFrame) {
  CefPageModelCache cache;
  const int frame_id = 1;

  cache.PutSnapshot(frame_id, "key", "data");
  cache.PutSelector(frame_id, "#el", true, 5, "span");

  CefPageModelCache::SnapshotEntry snap;
  EXPECT_TRUE(cache.GetCachedSnapshot(frame_id, "key", &snap));

  cache.InvalidateFrame(frame_id);

  EXPECT_FALSE(cache.GetCachedSnapshot(frame_id, "key", &snap));

  CefPageModelCache::SelectorEntry sel;
  EXPECT_FALSE(cache.GetCachedSelector(frame_id, "#el", &sel));
}

TEST(PageModelCache, InvalidateAll) {
  CefPageModelCache cache;

  cache.PutSnapshot(/*frame_id=*/1, "key", "data1");
  cache.PutSnapshot(/*frame_id=*/2, "key", "data2");
  cache.PutSelector(/*frame_id=*/3, "#x", true, 1, "a");

  cache.InvalidateAll();

  CefPageModelCache::SnapshotEntry snap;
  EXPECT_FALSE(cache.GetCachedSnapshot(1, "key", &snap));
  EXPECT_FALSE(cache.GetCachedSnapshot(2, "key", &snap));

  CefPageModelCache::SelectorEntry sel;
  EXPECT_FALSE(cache.GetCachedSelector(3, "#x", &sel));
}

TEST(PageModelCache, SeparateFrames) {
  CefPageModelCache cache;

  cache.PutSnapshot(/*frame_id=*/1, "key", "frame1_data");
  cache.PutSnapshot(/*frame_id=*/2, "key", "frame2_data");

  CefPageModelCache::SnapshotEntry entry1;
  EXPECT_TRUE(cache.GetCachedSnapshot(1, "key", &entry1));
  EXPECT_EQ("frame1_data", entry1.snapshot_text);

  CefPageModelCache::SnapshotEntry entry2;
  EXPECT_TRUE(cache.GetCachedSnapshot(2, "key", &entry2));
  EXPECT_EQ("frame2_data", entry2.snapshot_text);

  // Invalidating frame 1 should not affect frame 2.
  cache.InvalidateFrame(1);
  EXPECT_FALSE(cache.GetCachedSnapshot(1, "key", &entry1));
  EXPECT_TRUE(cache.GetCachedSnapshot(2, "key", &entry2));
  EXPECT_EQ("frame2_data", entry2.snapshot_text);
}
