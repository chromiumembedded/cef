// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/action_trace.h"

#include <algorithm>

#include "base/strings/stringprintf.h"

namespace {

// Truncate a string to max_len chars, appending "..." if truncated.
std::string Truncate(const std::string& s, size_t max_len) {
  if (s.size() <= max_len) {
    return s;
  }
  if (max_len <= 3) {
    return s.substr(0, max_len);
  }
  return s.substr(0, max_len - 3) + "...";
}

// Format a duration as a human-readable string.
std::string FormatDuration(base::TimeDelta duration) {
  double seconds = duration.InMillisecondsF() / 1000.0;
  if (seconds < 0.1) {
    return base::StringPrintf("%.0fms", duration.InMillisecondsF());
  }
  return base::StringPrintf("%.1fs", seconds);
}

// Find an action by ID in the vector. Returns nullptr if not found.
CefActionTrace::Action* FindAction(std::vector<CefActionTrace::Action>& actions,
                                   int action_id) {
  for (auto& action : actions) {
    if (action.id == action_id) {
      return &action;
    }
  }
  return nullptr;
}

}  // namespace

CefActionTrace::CefActionTrace() = default;
CefActionTrace::~CefActionTrace() = default;

void CefActionTrace::SetLevel(Level level) {
  base::AutoLock lock(lock_);
  level_ = level;
}

CefActionTrace::Level CefActionTrace::GetLevel() const {
  base::AutoLock lock(lock_);
  return level_;
}

int CefActionTrace::RecordAction(const std::string& type,
                                 const std::string& target,
                                 bool success,
                                 const std::string& error,
                                 const std::string& result_summary) {
  base::AutoLock lock(lock_);
  Action action;
  action.id = next_id_++;
  action.type = type;
  action.target = target;
  action.success = success;
  action.error = error;
  action.result_summary = Truncate(result_summary, 200);
  action.started_at = base::TimeTicks::Now();
  action.completed_at = action.started_at;
  action.duration = base::TimeDelta();
  actions_.push_back(std::move(action));
  return actions_.back().id;
}

void CefActionTrace::AttachSnapshot(int action_id,
                                    const std::string& snapshot_before,
                                    const std::string& snapshot_after) {
  base::AutoLock lock(lock_);
  if (level_ != Level::kFull) {
    return;
  }
  Action* action = FindAction(actions_, action_id);
  if (!action) {
    return;
  }
  action->snapshot_before = snapshot_before;
  action->snapshot_after = snapshot_after;
}

void CefActionTrace::AttachScreenshot(int action_id,
                                      const std::string& screenshot_path) {
  base::AutoLock lock(lock_);
  if (level_ != Level::kFull) {
    return;
  }
  Action* action = FindAction(actions_, action_id);
  if (!action) {
    return;
  }
  action->screenshot_path = screenshot_path;
}

void CefActionTrace::AttachMetadata(int action_id,
                                    base::Value::Dict metadata) {
  base::AutoLock lock(lock_);
  if (level_ != Level::kFull) {
    return;
  }
  Action* action = FindAction(actions_, action_id);
  if (!action) {
    return;
  }
  action->metadata = std::move(metadata);
}

int CefActionTrace::BeginAction(const std::string& type,
                                const std::string& target) {
  base::AutoLock lock(lock_);
  Action action;
  action.id = next_id_++;
  action.type = type;
  action.target = target;
  action.started_at = base::TimeTicks::Now();
  actions_.push_back(std::move(action));
  return actions_.back().id;
}

void CefActionTrace::EndAction(int action_id,
                               bool success,
                               const std::string& error,
                               const std::string& result_summary) {
  base::AutoLock lock(lock_);
  Action* action = FindAction(actions_, action_id);
  if (!action) {
    return;
  }
  action->completed_at = base::TimeTicks::Now();
  action->duration = action->completed_at - action->started_at;
  action->success = success;
  action->error = error;
  action->result_summary = Truncate(result_summary, 200);
}

CefActionTrace::Summary CefActionTrace::GetSummary() const {
  base::AutoLock lock(lock_);
  Summary summary;
  summary.total_actions = actions_.size();
  if (actions_.empty()) {
    return summary;
  }

  for (const auto& action : actions_) {
    if (action.success) {
      summary.succeeded++;
    } else {
      summary.failed++;
      summary.last_error = action.error;
    }
    summary.total_duration += action.duration;
  }

  summary.success_rate =
      summary.total_actions > 0
          ? static_cast<double>(summary.succeeded) / summary.total_actions
          : 0.0;
  summary.avg_duration = summary.total_actions > 0
                             ? summary.total_duration / summary.total_actions
                             : base::TimeDelta();
  return summary;
}

std::string CefActionTrace::GetTrace(Level level) const {
  base::AutoLock lock(lock_);

  if (actions_.empty()) {
    return "Trace: 0 actions";
  }

  // Compute summary stats.
  size_t succeeded = 0;
  size_t failed = 0;
  base::TimeDelta total_duration;
  std::string last_error;

  for (const auto& action : actions_) {
    if (action.success) {
      succeeded++;
    } else {
      failed++;
      last_error = action.error;
    }
    total_duration += action.duration;
  }

  double success_rate = actions_.size() > 0
                            ? 100.0 * succeeded / actions_.size()
                            : 0.0;

  std::string result = base::StringPrintf(
      "Trace: %zu actions, %zu passed, %zu failed (%.1f%%), %s total",
      actions_.size(), succeeded, failed, success_rate,
      FormatDuration(total_duration).c_str());

  if (!last_error.empty()) {
    result += "\nLast error: " + last_error;
  }

  if (level == Level::kSummary) {
    return result;
  }

  // Level 2+: append per-action detail.
  for (const auto& action : actions_) {
    const char* mark = action.success ? "\xe2\x9c\x93" : "\xe2\x9c\x97";
    std::string line = base::StringPrintf(
        "\n  [%d] %s %s %s (%s)", action.id, action.type.c_str(),
        action.target.c_str(), mark,
        FormatDuration(action.duration).c_str());

    if (!action.success && !action.error.empty()) {
      line += " \xe2\x86\x92 " + Truncate(action.error, 80);
    } else if (!action.result_summary.empty()) {
      line +=
          " \xe2\x86\x92 \"" + Truncate(action.result_summary, 80) + "\"";
    }

    result += line;

    // Level 3: append snapshot/screenshot data.
    if (level == Level::kFull) {
      if (!action.snapshot_before.empty()) {
        result += "\n      Before: \"" + Truncate(action.snapshot_before, 200) +
                  "\"";
      }
      if (!action.snapshot_after.empty()) {
        result += "\n      After:  \"" + Truncate(action.snapshot_after, 200) +
                  "\"";
      }
      if (!action.screenshot_path.empty()) {
        result += "\n      Screenshot: " + action.screenshot_path;
      }
    }
  }

  return result;
}

base::Value::Dict CefActionTrace::GetTraceAsDict(Level level) const {
  base::AutoLock lock(lock_);

  base::Value::Dict dict;

  size_t succeeded = 0;
  size_t failed = 0;
  base::TimeDelta total_duration;

  for (const auto& action : actions_) {
    if (action.success) {
      succeeded++;
    } else {
      failed++;
    }
    total_duration += action.duration;
  }

  dict.Set("total", static_cast<int>(actions_.size()));
  dict.Set("succeeded", static_cast<int>(succeeded));
  dict.Set("failed", static_cast<int>(failed));
  dict.Set("success_rate",
           actions_.size() > 0
               ? static_cast<double>(succeeded) / actions_.size()
               : 0.0);
  dict.Set("duration_ms", static_cast<int>(total_duration.InMilliseconds()));

  if (level >= Level::kDetail) {
    base::Value::List action_list;
    for (const auto& action : actions_) {
      base::Value::Dict action_dict;
      action_dict.Set("id", action.id);
      action_dict.Set("type", action.type);
      action_dict.Set("target", action.target);
      action_dict.Set("success", action.success);
      action_dict.Set("duration_ms",
                      static_cast<int>(action.duration.InMilliseconds()));

      if (!action.error.empty()) {
        action_dict.Set("error", action.error);
      }
      if (!action.result_summary.empty()) {
        action_dict.Set("result", action.result_summary);
      }

      if (level == Level::kFull) {
        if (!action.snapshot_before.empty()) {
          action_dict.Set("snapshot_before", action.snapshot_before);
        }
        if (!action.snapshot_after.empty()) {
          action_dict.Set("snapshot_after", action.snapshot_after);
        }
        if (!action.screenshot_path.empty()) {
          action_dict.Set("screenshot_path", action.screenshot_path);
        }
        if (!action.metadata.empty()) {
          action_dict.Set("metadata", action.metadata.Clone());
        }
      }

      action_list.Append(std::move(action_dict));
    }
    dict.Set("actions", std::move(action_list));
  }

  return dict;
}

std::vector<CefActionTrace::Action> CefActionTrace::GetRecentActions(
    size_t count) const {
  base::AutoLock lock(lock_);
  if (actions_.empty() || count == 0) {
    return {};
  }
  size_t start = actions_.size() > count ? actions_.size() - count : 0;
  std::vector<Action> result;
  result.reserve(actions_.size() - start);
  for (size_t i = start; i < actions_.size(); ++i) {
    Action copy;
    copy.id = actions_[i].id;
    copy.type = actions_[i].type;
    copy.target = actions_[i].target;
    copy.success = actions_[i].success;
    copy.error = actions_[i].error;
    copy.result_summary = actions_[i].result_summary;
    copy.started_at = actions_[i].started_at;
    copy.completed_at = actions_[i].completed_at;
    copy.duration = actions_[i].duration;
    copy.snapshot_before = actions_[i].snapshot_before;
    copy.snapshot_after = actions_[i].snapshot_after;
    copy.screenshot_path = actions_[i].screenshot_path;
    copy.metadata = actions_[i].metadata.Clone();
    result.push_back(std::move(copy));
  }
  return result;
}

std::vector<CefActionTrace::Action> CefActionTrace::GetAllActions() const {
  base::AutoLock lock(lock_);
  std::vector<Action> result;
  result.reserve(actions_.size());
  for (const auto& action : actions_) {
    Action copy;
    copy.id = action.id;
    copy.type = action.type;
    copy.target = action.target;
    copy.success = action.success;
    copy.error = action.error;
    copy.result_summary = action.result_summary;
    copy.started_at = action.started_at;
    copy.completed_at = action.completed_at;
    copy.duration = action.duration;
    copy.snapshot_before = action.snapshot_before;
    copy.snapshot_after = action.snapshot_after;
    copy.screenshot_path = action.screenshot_path;
    copy.metadata = action.metadata.Clone();
    result.push_back(std::move(copy));
  }
  return result;
}

void CefActionTrace::Clear() {
  base::AutoLock lock(lock_);
  actions_.clear();
  next_id_ = 1;
}

size_t CefActionTrace::GetActionCount() const {
  base::AutoLock lock(lock_);
  return actions_.size();
}
