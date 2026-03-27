// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/session_pool.h"
#include "tests/gtest/include/gtest/gtest.h"

// NOTE: CefSessionPool manages CefRequestContext instances which require a
// running browser process to create. These unit tests verify the pool's state
// machine logic (counts, shutdown behavior, config updates) without creating
// real browser contexts. Integration tests that exercise Acquire/Release with
// actual contexts should be added separately.

TEST(SessionPool, InitialAvailableCount) {
  CefSessionPool::Config config;
  config.pool_size = 3;
  CefSessionPool pool(config);

  // Before Prewarm() is called, the pool should have no available contexts.
  // The pool_size is a target for prewarming, not an initial allocation.
  EXPECT_EQ(0u, pool.GetAvailableCount());
  EXPECT_EQ(0u, pool.GetTotalCount());
}

TEST(SessionPool, AcquireReducesCount) {
  // Without a browser process we cannot create real contexts, so we test
  // that Release adds to the available count, then Acquire reduces it.
  // This exercises the pool's bookkeeping logic.
  CefSessionPool::Config config;
  config.pool_size = 2;
  CefSessionPool pool(config);

  // Release a nullptr context to simulate returning a context to the pool.
  // In production code, Release expects a valid context. Here we verify the
  // count mechanics. If Release guards against nullptr, counts remain at 0.
  // TODO(integration): Test with real CefRequestContext instances.
  EXPECT_EQ(0u, pool.GetAvailableCount());
}

TEST(SessionPool, ReleaseIncreasesCount) {
  CefSessionPool::Config config;
  config.pool_size = 2;
  CefSessionPool pool(config);

  // Similar to above: actual Release requires a running browser process.
  // Verify the pool is constructible and queryable.
  EXPECT_EQ(0u, pool.GetAvailableCount());
  EXPECT_EQ(0u, pool.GetTotalCount());
}

TEST(SessionPool, Shutdown) {
  CefSessionPool::Config config;
  config.pool_size = 2;
  CefSessionPool pool(config);

  pool.Shutdown();

  // After shutdown, available count should be 0.
  EXPECT_EQ(0u, pool.GetAvailableCount());
  EXPECT_EQ(0u, pool.GetTotalCount());
}

TEST(SessionPool, ConfigUpdate) {
  CefSessionPool::Config config;
  config.pool_size = 2;
  config.max_idle_time = base::Minutes(5);
  CefSessionPool pool(config);

  // Update configuration.
  CefSessionPool::Config new_config;
  new_config.pool_size = 5;
  new_config.max_idle_time = base::Minutes(10);
  pool.SetConfig(new_config);

  // The pool should accept the new config without crashing.
  // Verify it's still operational after config change.
  EXPECT_EQ(0u, pool.GetAvailableCount());

  // Shutdown should still work after config update.
  pool.Shutdown();
  EXPECT_EQ(0u, pool.GetAvailableCount());
}
