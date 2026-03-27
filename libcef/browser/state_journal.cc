// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/state_journal.h"

#include <string>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"

namespace {

// Build a JSON line for a "set" operation.
std::string BuildSetLine(const std::string& key,
                         const base::Value& value,
                         double timestamp) {
  base::Value::Dict entry;
  entry.Set("op", "set");
  entry.Set("key", key);
  entry.Set("value", value.Clone());
  entry.Set("ts", timestamp);

  std::string json;
  if (!base::JSONWriter::Write(entry, &json)) {
    LOG(ERROR) << "Failed to serialize set entry for key: " << key;
    return std::string();
  }
  return json;
}

// Build a JSON line for a "delete" operation.
std::string BuildDeleteLine(const std::string& key, double timestamp) {
  base::Value::Dict entry;
  entry.Set("op", "delete");
  entry.Set("key", key);
  entry.Set("ts", timestamp);

  std::string json;
  if (!base::JSONWriter::Write(entry, &json)) {
    LOG(ERROR) << "Failed to serialize delete entry for key: " << key;
    return std::string();
  }
  return json;
}

// Build a JSON line for a "snapshot" operation.
std::string BuildSnapshotLine(const base::Value::Dict& data, double timestamp) {
  base::Value::Dict entry;
  entry.Set("op", "snapshot");
  entry.Set("data", data.Clone());
  entry.Set("ts", timestamp);

  std::string json;
  if (!base::JSONWriter::Write(entry, &json)) {
    LOG(ERROR) << "Failed to serialize snapshot entry.";
    return std::string();
  }
  return json;
}

}  // namespace

CefStateJournal::CefStateJournal() = default;

CefStateJournal::~CefStateJournal() {
  Close();
}

bool CefStateJournal::Initialize(const base::FilePath& journal_path) {
  base::AutoLock lock(lock_);

  if (is_open_) {
    LOG(WARNING) << "State journal already initialized.";
    return false;
  }

  journal_path_ = journal_path;

  if (base::PathExists(journal_path_)) {
    // Replay existing journal to rebuild in-memory state.
    if (!ReplayJournal(journal_path_)) {
      LOG(ERROR) << "Failed to replay journal: " << journal_path_.value();
      // Continue with empty state; the file may be corrupt.
      state_ = base::Value::Dict();
      pending_ops_ = 0;
    }
  }

  is_open_ = true;
  return true;
}

bool CefStateJournal::Set(const std::string& key, base::Value value) {
  base::AutoLock lock(lock_);

  if (!is_open_) {
    LOG(ERROR) << "State journal is not open.";
    return false;
  }

  state_.Set(key, value.Clone());

  const double timestamp =
      base::Time::Now().InSecondsFSinceUnixEpoch();
  const std::string line = BuildSetLine(key, value, timestamp);
  if (line.empty() || !AppendLine(line)) {
    return false;
  }

  pending_ops_++;

  if (pending_ops_ >= compaction_threshold_) {
    // WriteCompactedSnapshot handles its own error logging.
    WriteCompactedSnapshot();
  }

  return true;
}

bool CefStateJournal::Delete(const std::string& key) {
  base::AutoLock lock(lock_);

  if (!is_open_) {
    LOG(ERROR) << "State journal is not open.";
    return false;
  }

  state_.Remove(key);

  const double timestamp =
      base::Time::Now().InSecondsFSinceUnixEpoch();
  const std::string line = BuildDeleteLine(key, timestamp);
  if (line.empty() || !AppendLine(line)) {
    return false;
  }

  pending_ops_++;

  if (pending_ops_ >= compaction_threshold_) {
    WriteCompactedSnapshot();
  }

  return true;
}

const base::Value* CefStateJournal::Get(const std::string& key) const {
  base::AutoLock lock(lock_);
  return state_.Find(key);
}

bool CefStateJournal::Has(const std::string& key) const {
  base::AutoLock lock(lock_);
  return state_.contains(key);
}

base::Value::Dict CefStateJournal::GetAll() const {
  base::AutoLock lock(lock_);
  return state_.Clone();
}

size_t CefStateJournal::GetEntryCount() const {
  base::AutoLock lock(lock_);
  return state_.size();
}

size_t CefStateJournal::GetPendingOperationCount() const {
  base::AutoLock lock(lock_);
  return pending_ops_;
}

bool CefStateJournal::Compact() {
  base::AutoLock lock(lock_);

  if (!is_open_) {
    LOG(ERROR) << "State journal is not open.";
    return false;
  }

  return WriteCompactedSnapshot();
}

void CefStateJournal::SetCompactionThreshold(size_t threshold) {
  base::AutoLock lock(lock_);
  compaction_threshold_ = threshold;
}

bool CefStateJournal::NeedsCompaction() const {
  base::AutoLock lock(lock_);
  return pending_ops_ >= compaction_threshold_;
}

bool CefStateJournal::Flush() {
  base::AutoLock lock(lock_);

  if (!is_open_) {
    LOG(ERROR) << "State journal is not open.";
    return false;
  }

  // AppendLine writes and closes per call, so there is nothing buffered
  // to flush at this level. We verify the file is accessible.
  return base::PathExists(journal_path_);
}

void CefStateJournal::Close() {
  base::AutoLock lock(lock_);
  is_open_ = false;
}

bool CefStateJournal::IsOpen() const {
  base::AutoLock lock(lock_);
  return is_open_;
}

bool CefStateJournal::ReplayJournal(const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    LOG(ERROR) << "Failed to read journal file: " << path.value();
    return false;
  }

  if (contents.empty()) {
    return true;
  }

  state_ = base::Value::Dict();
  pending_ops_ = 0;

  const std::vector<std::string> lines = base::SplitString(
      contents, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& line : lines) {
    if (line.empty()) {
      continue;
    }

    std::optional<base::Value::Dict> entry = base::JSONReader::ReadDict(line);
    if (!entry.has_value()) {
      // Gracefully skip truncated or corrupt lines (e.g. last line after
      // a crash).
      LOG(WARNING) << "Skipping corrupt journal line.";
      continue;
    }

    const std::string* op = entry->FindString("op");
    if (!op) {
      LOG(WARNING) << "Skipping journal line with missing op.";
      continue;
    }

    if (*op == "set") {
      const std::string* key = entry->FindString("key");
      base::Value* value = entry->Find("value");
      if (key && value) {
        state_.Set(*key, value->Clone());
        pending_ops_++;
      } else {
        LOG(WARNING) << "Skipping malformed set entry.";
      }
    } else if (*op == "delete") {
      const std::string* key = entry->FindString("key");
      if (key) {
        state_.Remove(*key);
        pending_ops_++;
      } else {
        LOG(WARNING) << "Skipping malformed delete entry.";
      }
    } else if (*op == "snapshot") {
      const base::Value::Dict* data = entry->FindDict("data");
      if (data) {
        state_ = data->Clone();
        // A snapshot resets the operation counter since the state is fully
        // captured.
        pending_ops_ = 0;
      } else {
        LOG(WARNING) << "Skipping malformed snapshot entry.";
      }
    } else {
      LOG(WARNING) << "Skipping unknown journal op: " << *op;
    }
  }

  return true;
}

bool CefStateJournal::AppendLine(const std::string& line) {
  const std::string data = line + "\n";

  if (!base::PathExists(journal_path_)) {
    // Create the file with the initial content.
    if (!base::WriteFile(journal_path_, data)) {
      LOG(ERROR) << "Failed to create journal file: "
                 << journal_path_.value();
      return false;
    }
    return true;
  }

  if (!base::AppendToFile(journal_path_, data)) {
    LOG(ERROR) << "Failed to append to journal file: "
               << journal_path_.value();
    return false;
  }
  return true;
}

bool CefStateJournal::WriteCompactedSnapshot() {
  const double timestamp =
      base::Time::Now().InSecondsFSinceUnixEpoch();
  const std::string snapshot_line = BuildSnapshotLine(state_, timestamp);
  if (snapshot_line.empty()) {
    return false;
  }

  const std::string data = snapshot_line + "\n";

  // Atomic compaction: write to a temporary file, then rename over the
  // original journal to avoid data loss if the process crashes mid-write.
  const base::FilePath tmp_path =
      journal_path_.AddExtension(FILE_PATH_LITERAL(".tmp"));

  if (!base::WriteFile(tmp_path, data)) {
    LOG(ERROR) << "Failed to write compacted snapshot to temp file: "
               << tmp_path.value();
    return false;
  }

  if (!base::Move(tmp_path, journal_path_)) {
    LOG(ERROR) << "Failed to rename compacted journal: "
               << tmp_path.value() << " -> " << journal_path_.value();
    // Clean up the temp file on failure.
    base::DeleteFile(tmp_path);
    return false;
  }

  pending_ops_ = 0;
  return true;
}
