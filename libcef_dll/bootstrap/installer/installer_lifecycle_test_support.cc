// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle_test_support.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"

namespace cef_installer {
namespace test {

namespace {

constexpr char kRelaunchStartedEvent[] = "relaunch_started";
constexpr char kOperationResultEvent[] = "operation_result";
constexpr size_t kDefaultCorrelationCapacity = 256;
constexpr base::TimeDelta kDefaultCorrelationExpiration = base::Minutes(30);

std::optional<uint32_t> ParseJsonUint32(const base::Value* value) {
  if (!value) {
    return std::nullopt;
  }
  double number = 0;
  if (value->is_int()) {
    number = value->GetInt();
  } else if (value->is_double()) {
    number = value->GetDouble();
  } else {
    return std::nullopt;
  }
  if (!std::isfinite(number) || number < 1 ||
      number > std::numeric_limits<DWORD>::max() ||
      std::floor(number) != number) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(number);
}

}  // namespace

ParsedInstallerLifecycleEvent ParseInstallerLifecyclePayload(
    std::string_view payload_with_nul) {
  ParsedInstallerLifecycleEvent parsed;
  if (payload_with_nul.size() < 2 ||
      payload_with_nul.size() > kInstallerLifecycleMaxPayloadSize ||
      payload_with_nul.back() != '\0' ||
      payload_with_nul.find('\0') != payload_with_nul.size() - 1) {
    return parsed;
  }
  const std::string_view json =
      payload_with_nul.substr(0, payload_with_nul.size() - 1);
  if (!base::IsStringUTF8(json)) {
    return parsed;
  }
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!dict) {
    return parsed;
  }
  std::optional<int> protocol_version = dict->FindInt("protocol_version");
  const std::string* event = dict->FindString("event");
  if (!protocol_version || !event) {
    return parsed;
  }
  if (*protocol_version != kInstallerLifecycleProtocolVersion ||
      (*event != kRelaunchStartedEvent && *event != kOperationResultEvent)) {
    parsed.status = InstallerLifecycleParseStatus::kIgnored;
    return parsed;
  }
  const std::string* operation_id = dict->FindString("operation_id");
  if (!operation_id || !IsValidInstallerLifecycleOperationId(*operation_id)) {
    return parsed;
  }
  parsed.operation_id = *operation_id;

  if (*event == kRelaunchStartedEvent) {
    std::optional<uint32_t> child_pid =
        ParseJsonUint32(dict->Find("child_pid"));
    if (!child_pid) {
      return parsed;
    }
    parsed.kind = InstallerLifecycleEventKind::kRelaunchStarted;
    parsed.child_pid = *child_pid;
    parsed.status = InstallerLifecycleParseStatus::kValid;
    return parsed;
  }

  const std::string* command = dict->FindString("command");
  std::optional<bool> success = dict->FindBool("success");
  const std::string* outcome_name = dict->FindString("outcome");
  std::optional<int> exit_code = dict->FindInt("exit_code");
  if (!command || *command != "uninstall" || !success || !outcome_name ||
      !exit_code) {
    return parsed;
  }
  std::optional<Outcome> outcome = internal::StringToOutcome(*outcome_name);
  if (!outcome) {
    return parsed;
  }

  Result result;
  result.success = *success;
  result.outcome = *outcome;
  if (result.success) {
    if (result.outcome == Outcome::kFailed || *exit_code != kExitCodeSuccess ||
        dict->contains("error_code") || dict->contains("error_name") ||
        dict->contains("error_message")) {
      return parsed;
    }
  } else {
    std::optional<int> error_code = dict->FindInt("error_code");
    const std::string* error_name = dict->FindString("error_name");
    const std::string* error_message = dict->FindString("error_message");
    if (result.outcome != Outcome::kFailed || !error_code ||
        !IsValidInstallerLifecycleTerminalErrorCode(*error_code) ||
        *error_code != *exit_code || !error_name || !error_message ||
        error_message->size() > kInstallerLifecycleMaxErrorMessageSize ||
        *error_name != internal::ExitCodeToString(*error_code)) {
      return parsed;
    }
    result.error_code = *error_code;
    result.error_message = *error_message;
  }
  if (const base::ListValue* warnings = dict->FindList("warnings")) {
    if (warnings->size() > kInstallerLifecycleMaxWarnings) {
      return parsed;
    }
    for (const auto& warning : *warnings) {
      if (!warning.is_string() ||
          warning.GetString().size() > kInstallerLifecycleMaxWarningSize) {
        return parsed;
      }
      result.warnings.push_back(warning.GetString());
    }
  } else if (dict->contains("warnings")) {
    return parsed;
  }
  if (dict->contains("diagnostics_truncated") &&
      !dict->FindBool("diagnostics_truncated")) {
    return parsed;
  }

  parsed.kind = InstallerLifecycleEventKind::kOperationResult;
  parsed.exit_code = *exit_code;
  parsed.result = std::move(result);
  parsed.status = InstallerLifecycleParseStatus::kValid;
  return parsed;
}

InstallerLifecycleCorrelator::InstallerLifecycleCorrelator()
    : InstallerLifecycleCorrelator(
          kDefaultCorrelationCapacity,
          kDefaultCorrelationExpiration,
          base::BindRepeating([]() { return base::TimeTicks::Now(); })) {}

InstallerLifecycleCorrelator::InstallerLifecycleCorrelator(
    size_t capacity,
    base::TimeDelta expiration,
    NowCallback now_callback)
    : capacity_(std::max<size_t>(capacity, 1u)),
      expiration_(std::max(expiration, base::Milliseconds(1))),
      now_callback_(std::move(now_callback)) {}

base::TimeTicks InstallerLifecycleCorrelator::Now() const {
  return now_callback_ ? now_callback_.Run() : base::TimeTicks::Now();
}

size_t InstallerLifecycleCorrelator::PurgeExpired(base::TimeTicks now) {
  size_t removed = 0;
  for (auto entry = entries_.begin(); entry != entries_.end();) {
    if (now - entry->second.last_activity >= expiration_) {
      entry = entries_.erase(entry);
      ++removed;
    } else {
      ++entry;
    }
  }
  return removed;
}

size_t InstallerLifecycleCorrelator::PurgeExpired() {
  return PurgeExpired(Now());
}

bool InstallerLifecycleCorrelator::MakeRoomForNewEntry(bool terminal_first) {
  if (entries_.size() < capacity_) {
    return true;
  }

  auto oldest = entries_.end();
  for (auto entry = entries_.begin(); entry != entries_.end(); ++entry) {
    const bool orphan_terminal = entry->second.terminal.has_value() &&
                                 !entry->second.child_pid.has_value();
    if (!orphan_terminal) {
      continue;
    }
    if (oldest == entries_.end() ||
        entry->second.last_activity < oldest->second.last_activity) {
      oldest = entry;
    }
  }
  if (oldest == entries_.end()) {
    if (terminal_first) {
      return false;
    }
    oldest = std::min_element(entries_.begin(), entries_.end(),
                              [](const auto& left, const auto& right) {
                                return left.second.last_activity <
                                       right.second.last_activity;
                              });
  }
  entries_.erase(oldest);
  return true;
}

InstallerLifecycleCorrelationStatus InstallerLifecycleCorrelator::Accept(
    const ParsedInstallerLifecycleEvent& event) {
  if (event.status != InstallerLifecycleParseStatus::kValid) {
    return InstallerLifecycleCorrelationStatus::kIgnored;
  }
  const base::TimeTicks now = Now();
  PurgeExpired(now);
  auto entry = entries_.find(event.operation_id);

  if (event.kind == InstallerLifecycleEventKind::kRelaunchStarted) {
    if (entry != entries_.end() && entry->second.child_pid) {
      return InstallerLifecycleCorrelationStatus::kDuplicateIgnored;
    }
    if (entry == entries_.end()) {
      if (!MakeRoomForNewEntry(/*terminal_first=*/false)) {
        return InstallerLifecycleCorrelationStatus::kIgnored;
      }
      entry = entries_
                  .emplace(event.operation_id,
                           CorrelationEntry{.last_activity = now})
                  .first;
    }
    entry->second.child_pid = event.child_pid;
    entry->second.last_activity = now;
    if (entry->second.terminal) {
      return InstallerLifecycleCorrelationStatus::kTerminalCorrelated;
    }
    return InstallerLifecycleCorrelationStatus::kHandoffRecorded;
  }

  if (entry != entries_.end() && entry->second.terminal) {
    return InstallerLifecycleCorrelationStatus::kDuplicateIgnored;
  }
  if (entry == entries_.end()) {
    if (!MakeRoomForNewEntry(/*terminal_first=*/true)) {
      return InstallerLifecycleCorrelationStatus::kIgnored;
    }
    entry =
        entries_
            .emplace(event.operation_id, CorrelationEntry{.last_activity = now})
            .first;
  }
  entry->second.terminal = event;
  entry->second.last_activity = now;
  return entry->second.child_pid
             ? InstallerLifecycleCorrelationStatus::kTerminalCorrelated
             : InstallerLifecycleCorrelationStatus::kTerminalBuffered;
}

const ParsedInstallerLifecycleEvent*
InstallerLifecycleCorrelator::GetCorrelatedResult(
    std::string_view operation_id) {
  PurgeExpired();
  auto entry = entries_.find(std::string(operation_id));
  return entry == entries_.end() || !entry->second.child_pid ||
                 !entry->second.terminal
             ? nullptr
             : &*entry->second.terminal;
}

std::optional<ParsedInstallerLifecycleEvent>
InstallerLifecycleCorrelator::TakeCorrelatedResult(
    std::string_view operation_id) {
  PurgeExpired();
  auto entry = entries_.find(std::string(operation_id));
  if (entry == entries_.end() || !entry->second.child_pid ||
      !entry->second.terminal) {
    return std::nullopt;
  }
  std::optional<ParsedInstallerLifecycleEvent> result(
      std::move(entry->second.terminal));
  entries_.erase(entry);
  return result;
}

bool InstallerLifecycleCorrelator::Remove(std::string_view operation_id) {
  PurgeExpired();
  return entries_.erase(std::string(operation_id)) != 0;
}

size_t InstallerLifecycleCorrelator::tracked_operation_count() {
  PurgeExpired();
  return entries_.size();
}

}  // namespace test
}  // namespace cef_installer
