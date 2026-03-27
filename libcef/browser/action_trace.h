// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ACTION_TRACE_H_
#define CEF_LIBCEF_BROWSER_ACTION_TRACE_H_
#pragma once

#include <string>
#include <vector>

#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"

// Three-level action trace system for agent debugging.
//
// Level 1 (Summary): Action count, pass/fail, total duration
// Level 2 (Detail):  Each action with type, target, result, timing
// Level 3 (Full):    Detail + DOM snapshots, screenshots, page state
//
// Records are stored per-browser and can be queried at any detail level.
// Thread-safe via internal locking.
class CefActionTrace {
 public:
  // Detail levels for trace output.
  enum class Level {
    kSummary = 1,  // Action count, pass/fail, duration
    kDetail = 2,   // Each action's type, target, result, timing
    kFull = 3,     // Detail + snapshots and page state
  };

  // A single recorded action.
  struct Action {
    // Unique sequential ID within this trace.
    int id = 0;

    // Action type: "navigate", "click", "type", "snapshot", "eval", etc.
    std::string type;

    // Target: URL, selector, ref, or JS expression.
    std::string target;

    // Whether the action succeeded.
    bool success = true;

    // Error message if failed.
    std::string error;

    // Result summary (truncated to 200 chars).
    std::string result_summary;

    // Timing.
    base::TimeTicks started_at;
    base::TimeTicks completed_at;
    base::TimeDelta duration;

    // Level 3 data (only populated when recording at full level).
    std::string snapshot_before;  // Page snapshot before action
    std::string snapshot_after;   // Page snapshot after action
    std::string screenshot_path;  // Screenshot path if captured
    base::Value::Dict metadata;   // Arbitrary extra data
  };

  CefActionTrace();
  ~CefActionTrace();

  CefActionTrace(const CefActionTrace&) = delete;
  CefActionTrace& operator=(const CefActionTrace&) = delete;

  // Set the recording level. Actions are always recorded, but Level 3
  // data (snapshots) is only captured when recording at kFull.
  void SetLevel(Level level);
  Level GetLevel() const;

  // Record an action. Returns the action ID.
  int RecordAction(const std::string& type,
                   const std::string& target,
                   bool success,
                   const std::string& error = std::string(),
                   const std::string& result_summary = std::string());

  // Attach Level 3 data to the most recently recorded action.
  void AttachSnapshot(int action_id,
                      const std::string& snapshot_before,
                      const std::string& snapshot_after);
  void AttachScreenshot(int action_id, const std::string& screenshot_path);
  void AttachMetadata(int action_id, base::Value::Dict metadata);

  // Start timing an action (call before execution).
  int BeginAction(const std::string& type, const std::string& target);

  // End timing and record result (call after execution).
  void EndAction(int action_id, bool success,
                 const std::string& error = std::string(),
                 const std::string& result_summary = std::string());

  // Query the trace at a specific detail level.
  // Returns formatted text appropriate for the level.
  std::string GetTrace(Level level) const;

  // Get trace as structured data (for JSON serialization).
  base::Value::Dict GetTraceAsDict(Level level) const;

  // Get the last N actions.
  std::vector<Action> GetRecentActions(size_t count) const;

  // Get all actions.
  std::vector<Action> GetAllActions() const;

  // Get summary statistics.
  struct Summary {
    size_t total_actions = 0;
    size_t succeeded = 0;
    size_t failed = 0;
    double success_rate = 0.0;
    base::TimeDelta total_duration;
    base::TimeDelta avg_duration;
    std::string last_error;
  };
  Summary GetSummary() const;

  // Clear all recorded actions.
  void Clear();

  // Get the action count.
  size_t GetActionCount() const;

 private:
  mutable base::Lock lock_;
  Level level_ = Level::kDetail;
  std::vector<Action> actions_;
  int next_id_ = 1;
};

#endif  // CEF_LIBCEF_BROWSER_ACTION_TRACE_H_
