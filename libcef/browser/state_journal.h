// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_STATE_JOURNAL_H_
#define CEF_LIBCEF_BROWSER_STATE_JOURNAL_H_
#pragma once

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"

// Incremental state journal for efficient persistence.
//
// Instead of writing full JSON snapshots on every save, the journal appends
// only changed entries. Periodically compacts the journal into a single
// snapshot to bound file size and recovery time.
//
// Journal file format (newline-delimited JSON):
//   {"op":"set","key":"cookies/example.com","value":{...},"ts":1234567890}
//   {"op":"delete","key":"cookies/example.com/session_id","ts":1234567891}
//   {"op":"snapshot","data":{...},"ts":1234567892}
//
// Thread-safe via internal locking.
class CefStateJournal {
 public:
  CefStateJournal();
  ~CefStateJournal();

  CefStateJournal(const CefStateJournal&) = delete;
  CefStateJournal& operator=(const CefStateJournal&) = delete;

  // Initialize the journal with a file path. Replays existing entries.
  // Returns true if initialization succeeded.
  bool Initialize(const base::FilePath& journal_path);

  // Append a set operation to the journal.
  bool Set(const std::string& key, base::Value value);

  // Append a delete operation to the journal.
  bool Delete(const std::string& key);

  // Get the current value for a key, or nullptr if not set.
  const base::Value* Get(const std::string& key) const;

  // Check if a key exists.
  bool Has(const std::string& key) const;

  // Get all current key-value pairs as a dictionary.
  base::Value::Dict GetAll() const;

  // Get the number of entries in the current state.
  size_t GetEntryCount() const;

  // Get the number of journal operations since last compaction.
  size_t GetPendingOperationCount() const;

  // Compact the journal: write a full snapshot and truncate the journal.
  // Called automatically when pending operations exceed the threshold.
  bool Compact();

  // Set the threshold for automatic compaction (default: 100 operations).
  void SetCompactionThreshold(size_t threshold);

  // Check if the journal needs compaction.
  bool NeedsCompaction() const;

  // Flush any buffered writes to disk.
  bool Flush();

  // Close the journal file.
  void Close();

  // Check if the journal is initialized and open.
  bool IsOpen() const;

 private:
  // Replay journal entries from file to rebuild in-memory state.
  bool ReplayJournal(const base::FilePath& path);

  // Append a raw journal line to the file.
  bool AppendLine(const std::string& line);

  // Write a compacted snapshot to replace the journal.
  bool WriteCompactedSnapshot();

  mutable base::Lock lock_;
  base::FilePath journal_path_;
  bool is_open_ = false;

  // In-memory state rebuilt from journal.
  base::Value::Dict state_;

  // Count of operations since last compaction.
  size_t pending_ops_ = 0;
  size_t compaction_threshold_ = 100;
};

#endif  // CEF_LIBCEF_BROWSER_STATE_JOURNAL_H_
