// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <type_traits>

#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_logger.h"
#include "cef/libcef_dll/bootstrap/installer/installer_result_json.h"

namespace cef_installer {

namespace {

constexpr char kRelaunchStartedEvent[] = "relaunch_started";
constexpr char kOperationResultEvent[] = "operation_result";
constexpr char kDiagnosticFallback[] = "Diagnostic unavailable";

static_assert(std::numeric_limits<DWORD>::max() <= UINT64_C(9007199254740991));
static_assert(std::numeric_limits<int>::digits <= 53);
static_assert(std::is_same_v<decltype(COPYDATASTRUCT::dwData), ULONG_PTR>);
static_assert(sizeof(kWmCopyDataInstallerLifecycle) <=
              sizeof(COPYDATASTRUCT::dwData));

bool SerializeDictionary(const base::DictValue& dict, std::string* json) {
  return base::JSONWriter::Write(dict, json) &&
         json->size() + 1 <= kInstallerLifecycleMaxPayloadSize;
}

bool FitsWithTruncationFlag(const base::DictValue& dict) {
  base::DictValue candidate = dict.Clone();
  candidate.Set("diagnostics_truncated", true);
  std::string json;
  return SerializeDictionary(candidate, &json);
}

std::string BoundDiagnostic(std::string_view input,
                            size_t max_size,
                            bool* truncated) {
  if (!base::IsStringUTF8(input)) {
    *truncated = true;
    return kDiagnosticFallback;
  }
  std::string_view bounded = base::TruncateUTF8ToByteSize(input, max_size);
  if (bounded.size() != input.size()) {
    *truncated = true;
  }
  return std::string(bounded);
}

std::optional<std::string> SerializeLifecycleDictionary(base::DictValue dict,
                                                        const Result* result) {
  bool diagnostics_truncated = false;
  std::string error_message;
  std::vector<std::string> warnings;
  if (result && !result->success) {
    error_message = BoundDiagnostic(result->error_message,
                                    kInstallerLifecycleMaxErrorMessageSize,
                                    &diagnostics_truncated);
  }
  if (result) {
    const size_t count =
        std::min(result->warnings.size(),
                 static_cast<size_t>(kInstallerLifecycleMaxWarnings));
    diagnostics_truncated |= count != result->warnings.size();
    warnings.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      warnings.push_back(BoundDiagnostic(result->warnings[i],
                                         kInstallerLifecycleMaxWarningSize,
                                         &diagnostics_truncated));
    }
  }

  dict.Remove("error_message");
  dict.Remove("warnings");
  if (result && !result->success) {
    dict.Set("error_message", error_message);
    if (!FitsWithTruncationFlag(dict)) {
      diagnostics_truncated = true;
      size_t low = 0;
      size_t high = error_message.size();
      while (low < high) {
        const size_t middle = low + (high - low + 1) / 2;
        base::DictValue candidate = dict.Clone();
        candidate.Set("error_message", std::string(base::TruncateUTF8ToByteSize(
                                           error_message, middle)));
        if (FitsWithTruncationFlag(candidate)) {
          low = middle;
        } else {
          high = middle - 1;
        }
      }
      dict.Set("error_message",
               std::string(base::TruncateUTF8ToByteSize(error_message, low)));
      if (!FitsWithTruncationFlag(dict)) {
        return std::nullopt;
      }
    }
  } else if (!FitsWithTruncationFlag(dict)) {
    return std::nullopt;
  }

  base::ListValue included_warnings;
  for (const auto& warning : warnings) {
    base::ListValue candidate_warnings = included_warnings.Clone();
    candidate_warnings.Append(warning);
    base::DictValue candidate = dict.Clone();
    candidate.Set("warnings", std::move(candidate_warnings));
    if (FitsWithTruncationFlag(candidate)) {
      included_warnings.Append(warning);
      continue;
    }

    diagnostics_truncated = true;
    size_t low = 0;
    size_t high = warning.size();
    while (low < high) {
      const size_t middle = low + (high - low + 1) / 2;
      base::ListValue shortened = included_warnings.Clone();
      shortened.Append(
          std::string(base::TruncateUTF8ToByteSize(warning, middle)));
      base::DictValue shortened_candidate = dict.Clone();
      shortened_candidate.Set("warnings", std::move(shortened));
      if (FitsWithTruncationFlag(shortened_candidate)) {
        low = middle;
      } else {
        high = middle - 1;
      }
    }
    const std::string shortened_warning(
        base::TruncateUTF8ToByteSize(warning, low));
    if (!shortened_warning.empty()) {
      included_warnings.Append(shortened_warning);
    }
    break;
  }
  if (included_warnings.size() != warnings.size()) {
    diagnostics_truncated = true;
  }
  if (!included_warnings.empty()) {
    dict.Set("warnings", std::move(included_warnings));
  }
  if (diagnostics_truncated) {
    dict.Set("diagnostics_truncated", true);
  }

  std::string json;
  if (!SerializeDictionary(dict, &json)) {
    return std::nullopt;
  }
  return json;
}

}  // namespace

bool UninstallLifecycleContext::valid() const {
  return parent_window && IsValidInstallerLifecycleOperationId(operation_id);
}

std::string GenerateInstallerLifecycleOperationId() {
  std::array<uint8_t, 16> random = {};
  base::RandBytes(random);
  return base::ToLowerASCII(base::HexEncode(random));
}

bool IsValidInstallerLifecycleOperationId(std::string_view operation_id) {
  if (operation_id.size() != 32) {
    return false;
  }
  return std::all_of(operation_id.begin(), operation_id.end(), [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
  });
}

bool IsValidInstallerLifecycleTerminalErrorCode(int exit_code) {
  switch (exit_code) {
    case kExitCodeConfigError:
    case kExitCodeNetworkError:
    case kExitCodeSignatureError:
    case kExitCodeNoMatchingVersion:
    case kExitCodeExtractionError:
    case kExitCodeInstallError:
    case kExitCodeDatabaseError:
    case kExitCodeLockTimeout:
    case kExitCodeCancelled:
    case kExitCodeNoSentinel:
    case kExitCodeSentinelReadError:
    case kExitCodeSentinelOwnerMismatch:
    case kExitCodePolicyDenied:
    case kExitCodeIndexError:
    case kExitCodeRecoveryError:
    case kExitCodeRepairError:
    case kExitCodeQuarantineError:
    case kExitCodeRetentionSnapshotChanged:
    case kExitCodePolicyError:
    case kExitCodeUnknownError:
      return true;
    case kExitCodeSuccess:
    case kExitCodeRelaunched:
      return false;
  }
  return false;
}

std::optional<std::string> SerializeInstallerRelaunchStarted(
    std::string_view operation_id,
    DWORD child_pid) {
  if (!IsValidInstallerLifecycleOperationId(operation_id) || child_pid == 0) {
    return std::nullopt;
  }
  base::DictValue dict;
  dict.Set("protocol_version", kInstallerLifecycleProtocolVersion);
  dict.Set("event", kRelaunchStartedEvent);
  dict.Set("operation_id", operation_id);
  dict.Set("child_pid", static_cast<double>(child_pid));
  return SerializeLifecycleDictionary(std::move(dict), nullptr);
}

std::optional<std::string> SerializeInstallerOperationResult(
    std::string_view operation_id,
    const Result& result,
    int exit_code) {
  if (!IsValidInstallerLifecycleOperationId(operation_id) ||
      exit_code != ResultToExitCode(result) ||
      (!result.success &&
       !IsValidInstallerLifecycleTerminalErrorCode(exit_code))) {
    return std::nullopt;
  }
  // base::Value requires valid UTF-8 at construction. Substitute invalid
  // diagnostics before building the shared dictionary; the bounded helper
  // below uses the original Result so it can also set diagnostics_truncated.
  Result serializable_result = result;
  if (!serializable_result.success &&
      !base::IsStringUTF8(serializable_result.error_message)) {
    serializable_result.error_message = kDiagnosticFallback;
  }
  for (std::string& warning : serializable_result.warnings) {
    if (!base::IsStringUTF8(warning)) {
      warning = kDiagnosticFallback;
    }
  }
  base::DictValue dict = internal::BuildResultJsonValue(serializable_result);
  dict.Set("protocol_version", kInstallerLifecycleProtocolVersion);
  dict.Set("event", kOperationResultEvent);
  dict.Set("operation_id", operation_id);
  dict.Set("command", "uninstall");
  dict.Set("exit_code", exit_code);
  return SerializeLifecycleDictionary(std::move(dict), &result);
}

InstallerLifecycleSendStatus SendInstallerLifecycleMessage(
    HWND parent_window,
    std::string_view json) {
  if (!parent_window || !::IsWindow(parent_window)) {
    return InstallerLifecycleSendStatus::kInvalidWindow;
  }
  if (json.empty() || !base::IsStringUTF8(json) ||
      json.find('\0') != std::string_view::npos ||
      json.size() + 1 > kInstallerLifecycleMaxPayloadSize) {
    return InstallerLifecycleSendStatus::kSerializationError;
  }
  const std::string payload(json);
  COPYDATASTRUCT data = {};
  data.dwData = kWmCopyDataInstallerLifecycle;
  data.cbData = static_cast<DWORD>(json.size() + 1);
  data.lpData = const_cast<char*>(payload.c_str());
  DWORD_PTR receiver_result = 0;
  ::SetLastError(ERROR_SUCCESS);
  const LRESULT sent = ::SendMessageTimeoutW(
      parent_window, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
      SMTO_ABORTIFHUNG, kInstallerLifecycleSendTimeoutMs, &receiver_result);
  if (sent != 0) {
    return InstallerLifecycleSendStatus::kDelivered;
  }
  return ::GetLastError() == ERROR_TIMEOUT
             ? InstallerLifecycleSendStatus::kTimeout
             : InstallerLifecycleSendStatus::kError;
}

int FinalizeUninstallLifecycle(
    const std::optional<UninstallLifecycleContext>& context,
    const Result& result) {
  const int exit_code = ResultToExitCode(result);
  if (!context || !context->valid()) {
    return exit_code;
  }
  std::optional<std::string> json = SerializeInstallerOperationResult(
      context->operation_id, result, exit_code);
  const InstallerLifecycleSendStatus status =
      json ? SendInstallerLifecycleMessage(context->parent_window, *json)
           : InstallerLifecycleSendStatus::kSerializationError;
  if (status != InstallerLifecycleSendStatus::kDelivered) {
    Logger::GetInstance().Warning(
        "Uninstall lifecycle terminal delivery was not confirmed");
  }
  return exit_code;
}

int FinalizeControlledUninstallFailure(
    const std::optional<UninstallLifecycleContext>& context,
    ControlledUninstallFailure failure,
    int error_code) {
  const char* diagnostic = nullptr;
  switch (failure) {
    case ControlledUninstallFailure::kInvalidRetentionOptions:
      diagnostic = "Invalid retention option usage";
      break;
    case ControlledUninstallFailure::kInvalidRetentionAge:
      diagnostic = "Invalid retention age";
      break;
    case ControlledUninstallFailure::kConfigLoad:
      diagnostic = "Failed to load installer configuration";
      break;
    case ControlledUninstallFailure::kConfigNotFound:
      diagnostic = "Installer configuration was not found";
      break;
    case ControlledUninstallFailure::kExplicitModeDisabled:
      diagnostic = "Explicit installer command is not enabled";
      break;
    case ControlledUninstallFailure::kPreflightRejected:
      diagnostic = "Relaunched uninstall preflight was rejected";
      break;
  }
  return FinalizeUninstallLifecycle(context,
                                    Result::Error(error_code, diagnostic));
}

int FinalizeUninstallLifecycleAfterProgressUi(
    const std::optional<UninstallLifecycleContext>& context,
    const Result& result,
    base::OnceClosure close_progress_ui) {
  if (close_progress_ui) {
    std::move(close_progress_ui).Run();
  }
  return FinalizeUninstallLifecycle(context, result);
}

}  // namespace cef_installer
