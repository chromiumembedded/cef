// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "cef/libcef/browser/state_journal.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

base::FilePath CreateJournalPath(const base::ScopedTempDir& temp_dir) {
  return temp_dir.GetPath().AppendASCII("test_journal.jsonl");
}

}  // namespace

TEST(StateJournal, InitializeCreatesFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = CreateJournalPath(temp_dir);

  CefStateJournal journal;
  EXPECT_FALSE(journal.IsOpen());
  EXPECT_TRUE(journal.Initialize(path));
  EXPECT_TRUE(journal.IsOpen());
  EXPECT_TRUE(base::PathExists(path));

  journal.Close();
  EXPECT_FALSE(journal.IsOpen());
}

TEST(StateJournal, SetAndGet) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CefStateJournal journal;
  ASSERT_TRUE(journal.Initialize(CreateJournalPath(temp_dir)));

  EXPECT_TRUE(journal.Set("key1", base::Value("hello")));

  const base::Value* val = journal.Get("key1");
  ASSERT_TRUE(val);
  EXPECT_TRUE(val->is_string());
  EXPECT_EQ("hello", val->GetString());

  // Non-existent key returns nullptr.
  EXPECT_FALSE(journal.Get("nonexistent"));

  journal.Close();
}

TEST(StateJournal, DeleteKey) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CefStateJournal journal;
  ASSERT_TRUE(journal.Initialize(CreateJournalPath(temp_dir)));

  EXPECT_TRUE(journal.Set("key1", base::Value(42)));
  EXPECT_TRUE(journal.Has("key1"));

  EXPECT_TRUE(journal.Delete("key1"));
  EXPECT_FALSE(journal.Has("key1"));
  EXPECT_FALSE(journal.Get("key1"));

  journal.Close();
}

TEST(StateJournal, GetAll) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CefStateJournal journal;
  ASSERT_TRUE(journal.Initialize(CreateJournalPath(temp_dir)));

  EXPECT_TRUE(journal.Set("alpha", base::Value("a")));
  EXPECT_TRUE(journal.Set("beta", base::Value("b")));
  EXPECT_TRUE(journal.Set("gamma", base::Value("c")));

  base::Value::Dict all = journal.GetAll();
  EXPECT_EQ(3u, all.size());

  const std::string* alpha = all.FindString("alpha");
  ASSERT_TRUE(alpha);
  EXPECT_EQ("a", *alpha);

  const std::string* beta = all.FindString("beta");
  ASSERT_TRUE(beta);
  EXPECT_EQ("b", *beta);

  const std::string* gamma = all.FindString("gamma");
  ASSERT_TRUE(gamma);
  EXPECT_EQ("c", *gamma);

  EXPECT_EQ(3u, journal.GetEntryCount());

  journal.Close();
}

TEST(StateJournal, ReplayOnReopen) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = CreateJournalPath(temp_dir);

  // Write some entries and close.
  {
    CefStateJournal journal;
    ASSERT_TRUE(journal.Initialize(path));
    EXPECT_TRUE(journal.Set("persist1", base::Value("value1")));
    EXPECT_TRUE(journal.Set("persist2", base::Value(99)));
    EXPECT_TRUE(journal.Set("deleted", base::Value("gone")));
    EXPECT_TRUE(journal.Delete("deleted"));
    EXPECT_TRUE(journal.Flush());
    journal.Close();
  }

  // Re-open and verify state was replayed.
  {
    CefStateJournal journal;
    ASSERT_TRUE(journal.Initialize(path));

    const base::Value* v1 = journal.Get("persist1");
    ASSERT_TRUE(v1);
    EXPECT_EQ("value1", v1->GetString());

    const base::Value* v2 = journal.Get("persist2");
    ASSERT_TRUE(v2);
    EXPECT_EQ(99, v2->GetInt());

    EXPECT_FALSE(journal.Has("deleted"));
    EXPECT_EQ(2u, journal.GetEntryCount());

    journal.Close();
  }
}

TEST(StateJournal, Compaction) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = CreateJournalPath(temp_dir);

  CefStateJournal journal;
  ASSERT_TRUE(journal.Initialize(path));

  // Set a low compaction threshold for testing.
  journal.SetCompactionThreshold(5);

  // Write enough entries to trigger compaction.
  for (int i = 0; i < 6; i++) {
    EXPECT_TRUE(
        journal.Set("key" + std::to_string(i), base::Value(i)));
  }

  // After 6 operations with threshold 5, compaction should have run.
  // Pending ops should have been reset after compaction.
  EXPECT_LE(journal.GetPendingOperationCount(), 1u);

  // All data should still be correct after compaction.
  for (int i = 0; i < 6; i++) {
    const base::Value* val = journal.Get("key" + std::to_string(i));
    ASSERT_TRUE(val) << "key" << i << " missing after compaction";
    EXPECT_EQ(i, val->GetInt());
  }

  EXPECT_EQ(6u, journal.GetEntryCount());

  journal.Close();
}

TEST(StateJournal, CorruptLastLine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path = CreateJournalPath(temp_dir);

  // Write valid entries, then append a corrupt line.
  {
    CefStateJournal journal;
    ASSERT_TRUE(journal.Initialize(path));
    EXPECT_TRUE(journal.Set("good_key", base::Value("good_value")));
    EXPECT_TRUE(journal.Flush());
    journal.Close();
  }

  // Append corrupt data to the journal file.
  ASSERT_TRUE(base::AppendToFile(path, "{\"op\":\"set\",\"key\":\"bad\n"));

  // Re-open: should skip the corrupt last line and recover the valid state.
  {
    CefStateJournal journal;
    EXPECT_TRUE(journal.Initialize(path));

    const base::Value* val = journal.Get("good_key");
    ASSERT_TRUE(val);
    EXPECT_EQ("good_value", val->GetString());

    // The corrupt entry should not have been applied.
    EXPECT_FALSE(journal.Has("bad"));

    journal.Close();
  }
}

TEST(StateJournal, PendingOperationCount) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CefStateJournal journal;
  ASSERT_TRUE(journal.Initialize(CreateJournalPath(temp_dir)));

  // Set a high threshold so compaction doesn't auto-trigger.
  journal.SetCompactionThreshold(1000);

  EXPECT_EQ(0u, journal.GetPendingOperationCount());

  EXPECT_TRUE(journal.Set("a", base::Value(1)));
  EXPECT_EQ(1u, journal.GetPendingOperationCount());

  EXPECT_TRUE(journal.Set("b", base::Value(2)));
  EXPECT_EQ(2u, journal.GetPendingOperationCount());

  EXPECT_TRUE(journal.Delete("a"));
  EXPECT_EQ(3u, journal.GetPendingOperationCount());

  // Compact should reset the count.
  EXPECT_TRUE(journal.Compact());
  EXPECT_EQ(0u, journal.GetPendingOperationCount());

  journal.Close();
}
