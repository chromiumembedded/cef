// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/auth_vault_impl.h"
#include "cef/libcef/browser/storage_state_manager_impl.h"
#include "tests/gtest/include/gtest/gtest.h"

// Tests for dirty tracking in CefStorageStateManagerImpl.
// The manager defaults to dirty=true so the first save always proceeds.

TEST(DirtyTracking, InitiallyDirty) {
  CefRefPtr<CefStorageStateManagerImpl> manager =
      new CefStorageStateManagerImpl();
  EXPECT_TRUE(manager->IsDirty());
}

TEST(DirtyTracking, ClearDirty) {
  CefRefPtr<CefStorageStateManagerImpl> manager =
      new CefStorageStateManagerImpl();
  EXPECT_TRUE(manager->IsDirty());

  manager->ClearDirty();
  EXPECT_FALSE(manager->IsDirty());
}

TEST(DirtyTracking, MarkDirty) {
  CefRefPtr<CefStorageStateManagerImpl> manager =
      new CefStorageStateManagerImpl();

  manager->ClearDirty();
  EXPECT_FALSE(manager->IsDirty());

  manager->MarkDirty();
  EXPECT_TRUE(manager->IsDirty());
}

TEST(DirtyTracking, MarkDirtyWithOrigin) {
  CefRefPtr<CefStorageStateManagerImpl> manager =
      new CefStorageStateManagerImpl();

  manager->ClearDirty();

  manager->MarkDirty("https://example.com");
  EXPECT_TRUE(manager->IsDirty());

  const auto& origins = manager->GetModifiedOrigins();
  EXPECT_EQ(1u, origins.size());
  EXPECT_TRUE(origins.count("https://example.com"));

  // Adding another origin.
  manager->MarkDirty("https://other.com");
  EXPECT_EQ(2u, manager->GetModifiedOrigins().size());
  EXPECT_TRUE(manager->GetModifiedOrigins().count("https://other.com"));
}

TEST(DirtyTracking, ClearResetsOrigins) {
  CefRefPtr<CefStorageStateManagerImpl> manager =
      new CefStorageStateManagerImpl();

  manager->MarkDirty("https://a.com");
  manager->MarkDirty("https://b.com");
  EXPECT_EQ(2u, manager->GetModifiedOrigins().size());

  manager->ClearDirty();
  EXPECT_FALSE(manager->IsDirty());
  EXPECT_TRUE(manager->GetModifiedOrigins().empty());
}

// Tests for per-profile dirty tracking in CefAuthVaultImpl.

TEST(DirtyTracking, VaultProfileDirty) {
  CefRefPtr<CefAuthVaultImpl> vault = new CefAuthVaultImpl();

  const std::string profile_name = "test-profile";

  // Initially a profile should not be dirty.
  EXPECT_FALSE(vault->IsProfileDirty(profile_name));

  // Mark it dirty.
  vault->MarkProfileDirty(profile_name);
  EXPECT_TRUE(vault->IsProfileDirty(profile_name));

  // Clear it.
  vault->ClearProfileDirty(profile_name);
  EXPECT_FALSE(vault->IsProfileDirty(profile_name));

  // Multiple profiles are independent.
  vault->MarkProfileDirty("profile_a");
  vault->MarkProfileDirty("profile_b");
  EXPECT_TRUE(vault->IsProfileDirty("profile_a"));
  EXPECT_TRUE(vault->IsProfileDirty("profile_b"));

  vault->ClearProfileDirty("profile_a");
  EXPECT_FALSE(vault->IsProfileDirty("profile_a"));
  EXPECT_TRUE(vault->IsProfileDirty("profile_b"));
}
