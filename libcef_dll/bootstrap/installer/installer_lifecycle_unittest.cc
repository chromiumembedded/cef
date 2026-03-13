// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"

#include <windows.h>

#include <array>
#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle_test_support.h"
#include "cef/libcef_dll/bootstrap/installer/installer_result_json.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

using test::InstallerLifecycleCorrelationStatus;
using test::InstallerLifecycleCorrelator;
using test::InstallerLifecycleEventKind;
using test::InstallerLifecycleParseStatus;
using test::ParsedInstallerLifecycleEvent;
using test::ParseInstallerLifecyclePayload;

constexpr char kOperationId[] = "0123456789abcdef0123456789abcdef";
constexpr char kOtherOperationId[] = "fedcba9876543210fedcba9876543210";
constexpr char kThirdOperationId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

std::string WithNul(std::string json) {
  json.push_back('\0');
  return json;
}

class LifecycleWindow {
 public:
  explicit LifecycleWindow(LRESULT receiver_result = 1,
                           const bool* prerequisite = nullptr)
      : receiver_result_(receiver_result), prerequisite_(prerequisite) {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = ::GetModuleHandle(nullptr);
    window_class.lpszClassName = L"CefInstallerLifecycleUnitTest";
    ::RegisterClassW(&window_class);
    window_ =
        ::CreateWindowW(window_class.lpszClassName, L"", 0, 0, 0, 0, 0,
                        HWND_MESSAGE, nullptr, window_class.hInstance, this);
  }

  ~LifecycleWindow() {
    if (window_) {
      ::DestroyWindow(window_);
    }
  }

  HWND window() const { return window_; }
  int messages() const { return messages_; }
  const std::string& payload() const { return payload_; }
  bool prerequisite_observed() const { return prerequisite_observed_; }

 private:
  static LRESULT CALLBACK WindowProc(HWND window,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
    LifecycleWindow* self = reinterpret_cast<LifecycleWindow*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<LifecycleWindow*>(create->lpCreateParams);
      ::SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_COPYDATA && self) {
      auto* data = reinterpret_cast<COPYDATASTRUCT*>(lparam);
      if (data && data->dwData == kWmCopyDataInstallerLifecycle &&
          data->lpData) {
        ++self->messages_;
        self->prerequisite_observed_ =
            self->prerequisite_ && *self->prerequisite_;
        self->payload_.assign(static_cast<const char*>(data->lpData),
                              data->cbData);
        return self->receiver_result_;
      }
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
  }

  HWND window_ = nullptr;
  LRESULT receiver_result_ = 1;
  const bool* prerequisite_ = nullptr;
  bool prerequisite_observed_ = false;
  int messages_ = 0;
  std::string payload_;
};

class NonPumpingLifecycleWindow {
 public:
  NonPumpingLifecycleWindow()
      : ready_(::CreateEventW(nullptr, TRUE, FALSE, nullptr)),
        stop_(::CreateEventW(nullptr, TRUE, FALSE, nullptr)),
        thread_([this]() {
          WNDCLASSW window_class = {};
          window_class.lpfnWndProc = ::DefWindowProcW;
          window_class.hInstance = ::GetModuleHandle(nullptr);
          window_class.lpszClassName =
              L"CefInstallerNonPumpingLifecycleUnitTest";
          ::RegisterClassW(&window_class);
          window_ = ::CreateWindowW(window_class.lpszClassName, L"", 0, 0, 0, 0,
                                    0, HWND_MESSAGE, nullptr,
                                    window_class.hInstance, nullptr);
          ::SetEvent(ready_);
          ::WaitForSingleObject(stop_, INFINITE);
          if (window_) {
            ::DestroyWindow(window_);
          }
        }) {
    ::WaitForSingleObject(ready_, INFINITE);
  }

  ~NonPumpingLifecycleWindow() {
    ::SetEvent(stop_);
    thread_.join();
    ::CloseHandle(stop_);
    ::CloseHandle(ready_);
  }

  HWND window() const { return window_; }

 private:
  HANDLE ready_ = nullptr;
  HANDLE stop_ = nullptr;
  HWND window_ = nullptr;
  std::thread thread_;
};

TEST(InstallerLifecycleTest, ConstantsAndSchemaOperationId) {
  EXPECT_EQ(0x43454649u, kWmCopyDataInstallerProgress);
  EXPECT_EQ(0x43454652u, kWmCopyDataInstallerLifecycle);
  EXPECT_EQ(1, kInstallerLifecycleProtocolVersion);
  EXPECT_EQ(32u * 1024u, kInstallerLifecycleMaxPayloadSize);
  EXPECT_TRUE(IsValidInstallerLifecycleOperationId(kOperationId));
  EXPECT_FALSE(IsValidInstallerLifecycleOperationId("short"));
  EXPECT_FALSE(
      IsValidInstallerLifecycleOperationId("0123456789ABCDEF0123456789ABCDEF"));
  EXPECT_FALSE(
      IsValidInstallerLifecycleOperationId("g123456789abcdef0123456789abcdef"));
  const std::string first = GenerateInstallerLifecycleOperationId();
  const std::string second = GenerateInstallerLifecycleOperationId();
  EXPECT_TRUE(IsValidInstallerLifecycleOperationId(first));
  EXPECT_TRUE(IsValidInstallerLifecycleOperationId(second));
  EXPECT_NE(first, second);
}

TEST(InstallerLifecycleTest, TerminalErrorCodeSetIsExplicit) {
  for (const int exit_code :
       {kExitCodeConfigError,       kExitCodeNetworkError,
        kExitCodeSignatureError,    kExitCodeNoMatchingVersion,
        kExitCodeExtractionError,   kExitCodeInstallError,
        kExitCodeDatabaseError,     kExitCodeLockTimeout,
        kExitCodeCancelled,         kExitCodeNoSentinel,
        kExitCodeSentinelReadError, kExitCodeSentinelOwnerMismatch,
        kExitCodePolicyDenied,      kExitCodeIndexError,
        kExitCodeRecoveryError,     kExitCodeRepairError,
        kExitCodeQuarantineError,   kExitCodeRetentionSnapshotChanged,
        kExitCodePolicyError,       kExitCodeUnknownError}) {
    EXPECT_TRUE(IsValidInstallerLifecycleTerminalErrorCode(exit_code))
        << exit_code;
  }
  for (const int exit_code :
       {kExitCodeSuccess, kExitCodeRelaunched, 120, 198, 200, 9999}) {
    EXPECT_FALSE(IsValidInstallerLifecycleTerminalErrorCode(exit_code))
        << exit_code;
  }
}

TEST(InstallerLifecycleTest, ConstantsAndSchemaHandoffRoundTrip) {
  std::optional<std::string> json =
      SerializeInstallerRelaunchStarted(kOperationId, MAXDWORD);
  ASSERT_TRUE(json);
  EXPECT_LT(json->size() + 1, kInstallerLifecycleMaxPayloadSize);
  ParsedInstallerLifecycleEvent parsed =
      ParseInstallerLifecyclePayload(WithNul(*json));
  EXPECT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
  EXPECT_EQ(InstallerLifecycleEventKind::kRelaunchStarted, parsed.kind);
  EXPECT_EQ(kOperationId, parsed.operation_id);
  EXPECT_EQ(MAXDWORD, parsed.child_pid);
  EXPECT_FALSE(SerializeInstallerRelaunchStarted(kOperationId, 0));
}

TEST(InstallerLifecycleTest, ParseRejectsMalformedAndIgnoresUnknown) {
  const std::string valid = *SerializeInstallerRelaunchStarted(kOperationId, 1);
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(valid).status);
  EXPECT_EQ(
      InstallerLifecycleParseStatus::kMalformed,
      ParseInstallerLifecyclePayload(WithNul(valid) + std::string(1, '\0'))
          .status);
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(WithNul("[]")).status);
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(WithNul("{\"protocol_version\":1}"))
                .status);
  EXPECT_EQ(
      InstallerLifecycleParseStatus::kIgnored,
      ParseInstallerLifecyclePayload(
          WithNul("{\"protocol_version\":2,\"event\":\"relaunch_started\"}"))
          .status);
  EXPECT_EQ(InstallerLifecycleParseStatus::kIgnored,
            ParseInstallerLifecyclePayload(
                WithNul("{\"protocol_version\":1,\"event\":\"future\"}"))
                .status);
  std::string invalid_utf8 = "{\"protocol_version\":1,\"event\":\"";
  invalid_utf8.push_back(static_cast<char>(0xff));
  invalid_utf8 += "\"}";
  invalid_utf8.push_back('\0');
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(invalid_utf8).status);
}

TEST(InstallerLifecycleTest, ParseRejectsPidTypeAndRangeErrors) {
  const std::string prefix =
      "{\"protocol_version\":1,\"event\":\"relaunch_started\",";
  const std::string suffix =
      ",\"operation_id\":\"0123456789abcdef0123456789abcdef\"}";
  for (const std::string& pid : {"0", "-1", "1.5", "4294967296", "\"1\""}) {
    EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
              ParseInstallerLifecyclePayload(
                  WithNul(prefix + "\"child_pid\":" + pid + suffix))
                  .status)
        << pid;
  }
}

TEST(InstallerLifecycleTest, ParseEnforcesPayloadAndDiagnosticBounds) {
  const std::string handoff =
      *SerializeInstallerRelaunchStarted(kOperationId, 1);
  std::string exact = handoff;
  exact.append(kInstallerLifecycleMaxPayloadSize - handoff.size() - 1, ' ');
  EXPECT_EQ(InstallerLifecycleParseStatus::kValid,
            ParseInstallerLifecyclePayload(WithNul(exact)).status);
  exact.push_back(' ');
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(WithNul(exact)).status);

  Result failed = Result::Error(kExitCodeDatabaseError, "failed");
  std::string terminal = *SerializeInstallerOperationResult(
      kOperationId, failed, ResultToExitCode(failed));
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(terminal, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  dict->Set("error_message",
            std::string(kInstallerLifecycleMaxErrorMessageSize + 1, 'x'));
  std::string malformed;
  ASSERT_TRUE(base::JSONWriter::Write(*dict, &malformed));
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(WithNul(malformed)).status);

  dict = base::JSONReader::ReadDict(terminal, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  dict->Set("diagnostics_truncated", 1);
  ASSERT_TRUE(base::JSONWriter::Write(*dict, &malformed));
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(WithNul(malformed)).status);

  Result deferred = Result::Success({}, "");
  deferred.outcome = Outcome::kCleanupDeferred;
  terminal = *SerializeInstallerOperationResult(kOperationId, deferred, 0);
  EXPECT_EQ(InstallerLifecycleParseStatus::kValid,
            ParseInstallerLifecyclePayload(WithNul(terminal)).status);
}

TEST(InstallerLifecycleTest, ResultEnvelopePreservesNormalizedContract) {
  Result committed = Result::Success({}, "");
  Result deferred = Result::Success({}, "");
  deferred.outcome = Outcome::kCleanupDeferred;
  deferred.warnings.push_back("Pending trash reclamation remains");
  Result failed = Result::Error(kExitCodeDatabaseError, "Database save failed");
  Result cancelled =
      Result::Error(kExitCodeCancelled, "Operation cancelled by user");
  Result retention = Result::Success({}, "");
  retention.retention_max_age_days = 180;
  retention.retention_plan = RetentionPlan();

  for (const Result* result :
       {&committed, &deferred, &failed, &cancelled, &retention}) {
    const int exit_code = ResultToExitCode(*result);
    std::optional<std::string> json =
        SerializeInstallerOperationResult(kOperationId, *result, exit_code);
    ASSERT_TRUE(json);
    std::optional<base::DictValue> envelope =
        base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
    ASSERT_TRUE(envelope);
    envelope->Remove("protocol_version");
    envelope->Remove("event");
    envelope->Remove("operation_id");
    envelope->Remove("command");
    envelope->Remove("exit_code");
    EXPECT_EQ(internal::BuildResultJsonValue(*result), *envelope);

    ParsedInstallerLifecycleEvent parsed =
        ParseInstallerLifecyclePayload(WithNul(*json));
    ASSERT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
    ASSERT_TRUE(parsed.result);
    EXPECT_EQ(result->success, parsed.result->success);
    EXPECT_EQ(result->outcome, parsed.result->outcome);
    EXPECT_EQ(exit_code, parsed.exit_code);
  }
}

TEST(InstallerLifecycleTest, ResultEnvelopeRejectsMismatchedExitCode) {
  Result failed = Result::Error(kExitCodeDatabaseError, "failed");
  EXPECT_FALSE(SerializeInstallerOperationResult(kOperationId, failed,
                                                 kExitCodeSuccess));
  const std::string malformed = WithNul(
      "{\"protocol_version\":1,\"event\":\"operation_result\","
      "\"operation_id\":\"0123456789abcdef0123456789abcdef\","
      "\"command\":\"uninstall\",\"success\":false,"
      "\"outcome\":\"failed\",\"exit_code\":0,\"error_code\":106,"
      "\"error_name\":\"DATABASE_ERROR\",\"error_message\":\"x\"}");
  EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
            ParseInstallerLifecyclePayload(malformed).status);
}

TEST(InstallerLifecycleTest, ResultEnvelopeRejectsImpossibleTerminalCodes) {
  for (const int exit_code : {kExitCodeRelaunched, 120, 9999}) {
    const Result result = Result::Error(exit_code, "invalid child terminal");
    EXPECT_FALSE(
        SerializeInstallerOperationResult(kOperationId, result, exit_code))
        << exit_code;
  }

  Result valid = Result::Error(kExitCodeDatabaseError, "failed");
  std::optional<std::string> json = SerializeInstallerOperationResult(
      kOperationId, valid, kExitCodeDatabaseError);
  ASSERT_TRUE(json);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  for (const auto& test : std::to_array<std::pair<int, const char*>>({
           {kExitCodeRelaunched, "RELAUNCHED"},
           {120, "UNKNOWN_ERROR"},
           {9999, "UNKNOWN_ERROR"},
       })) {
    dict->Set("exit_code", test.first);
    dict->Set("error_code", test.first);
    dict->Set("error_name", test.second);
    std::string malformed;
    ASSERT_TRUE(base::JSONWriter::Write(*dict, &malformed));
    EXPECT_EQ(InstallerLifecycleParseStatus::kMalformed,
              ParseInstallerLifecyclePayload(WithNul(malformed)).status)
        << test.first;
  }

  const Result unknown =
      Result::Error(kExitCodeUnknownError, "classified unknown failure");
  json = SerializeInstallerOperationResult(kOperationId, unknown,
                                           kExitCodeUnknownError);
  ASSERT_TRUE(json);
  EXPECT_EQ(InstallerLifecycleParseStatus::kValid,
            ParseInstallerLifecyclePayload(WithNul(*json)).status);
}

TEST(InstallerLifecycleTest, ResultEnvelopeBoundsDiagnosticsAndUtf8) {
  Result result = Result::Error(kExitCodeDatabaseError, std::string(5000, 'e'));
  for (size_t i = 0; i < 40; ++i) {
    result.warnings.push_back(std::string(2000, 'w'));
  }
  std::optional<std::string> json = SerializeInstallerOperationResult(
      kOperationId, result, ResultToExitCode(result));
  ASSERT_TRUE(json);
  EXPECT_LE(json->size() + 1, kInstallerLifecycleMaxPayloadSize);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  EXPECT_TRUE(dict->FindBool("diagnostics_truncated").value_or(false));
  ASSERT_TRUE(dict->FindString("error_message"));
  EXPECT_LE(dict->FindString("error_message")->size(),
            kInstallerLifecycleMaxErrorMessageSize);
  ASSERT_TRUE(dict->FindList("warnings"));
  EXPECT_LE(dict->FindList("warnings")->size(), kInstallerLifecycleMaxWarnings);

  result.error_message.assign(1, static_cast<char>(0xff));
  result.warnings.clear();
  json = SerializeInstallerOperationResult(kOperationId, result,
                                           ResultToExitCode(result));
  ASSERT_TRUE(json);
  dict = base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  EXPECT_EQ("Diagnostic unavailable", *dict->FindString("error_message"));
  EXPECT_TRUE(dict->FindBool("diagnostics_truncated").value_or(false));

  result.error_message.clear();
  for (size_t i = 0; i < 2000; ++i) {
    result.error_message.append("\xE2\x82\xAC");
  }
  json = SerializeInstallerOperationResult(kOperationId, result,
                                           ResultToExitCode(result));
  ASSERT_TRUE(json);
  dict = base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  ASSERT_TRUE(dict->FindString("error_message"));
  EXPECT_TRUE(base::IsStringUTF8(*dict->FindString("error_message")));
  EXPECT_LE(dict->FindString("error_message")->size(),
            kInstallerLifecycleMaxErrorMessageSize);

  // Exercise tight total-payload boundaries where the final warning begins
  // with a multibyte code point. A non-useful empty truncation must be omitted.
  Result boundary = Result::Success({}, "");
  for (size_t i = 0; i < 31; ++i) {
    boundary.warnings.emplace_back(kInstallerLifecycleMaxWarningSize, 'a');
  }
  boundary.warnings.emplace_back();
  for (size_t i = 0; i < 341; ++i) {
    boundary.warnings.back().append("\xE2\x82\xAC");
  }
  json = SerializeInstallerOperationResult(kOperationId, boundary, 0);
  ASSERT_TRUE(json);
  dict = base::JSONReader::ReadDict(*json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict);
  EXPECT_TRUE(dict->FindBool("diagnostics_truncated").value_or(false));
  const base::ListValue* emitted = dict->FindList("warnings");
  ASSERT_TRUE(emitted);
  for (const auto& warning : *emitted) {
    ASSERT_TRUE(warning.is_string());
    EXPECT_FALSE(warning.GetString().empty());
    EXPECT_TRUE(base::IsStringUTF8(warning.GetString()));
  }
  EXPECT_TRUE(emitted->size() < boundary.warnings.size() ||
              (*emitted)[emitted->size() - 1].GetString().size() <
                  boundary.warnings.back().size());
}

TEST(InstallerLifecycleTest, CorrelationBuffersTerminalAndDeduplicates) {
  Result result = Result::Success({}, "");
  ParsedInstallerLifecycleEvent terminal = ParseInstallerLifecyclePayload(
      WithNul(*SerializeInstallerOperationResult(kOperationId, result, 0)));
  ParsedInstallerLifecycleEvent handoff = ParseInstallerLifecyclePayload(
      WithNul(*SerializeInstallerRelaunchStarted(kOperationId, 123)));
  InstallerLifecycleCorrelator correlator;
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalBuffered,
            correlator.Accept(terminal));
  EXPECT_FALSE(correlator.GetCorrelatedResult(kOperationId));
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kDuplicateIgnored,
            correlator.Accept(terminal));
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalCorrelated,
            correlator.Accept(handoff));
  EXPECT_TRUE(correlator.GetCorrelatedResult(kOperationId));
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kDuplicateIgnored,
            correlator.Accept(handoff));

  ParsedInstallerLifecycleEvent other_handoff = ParseInstallerLifecyclePayload(
      WithNul(*SerializeInstallerRelaunchStarted(kOtherOperationId, 456)));
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kHandoffRecorded,
            correlator.Accept(other_handoff));
  EXPECT_FALSE(correlator.GetCorrelatedResult(kOtherOperationId));
}

TEST(InstallerLifecycleTest, CorrelationIsBoundedExpiringAndConsumable) {
  base::TimeTicks now = base::TimeTicks::Now();
  InstallerLifecycleCorrelator correlator(
      2, base::Seconds(10),
      base::BindRepeating([](base::TimeTicks* value) { return *value; },
                          base::Unretained(&now)));
  Result result = Result::Success({}, "");
  const auto terminal = [&](std::string_view operation_id) {
    return ParseInstallerLifecyclePayload(
        WithNul(*SerializeInstallerOperationResult(operation_id, result, 0)));
  };
  const auto handoff = [](std::string_view operation_id, DWORD child_pid) {
    return ParseInstallerLifecyclePayload(
        WithNul(*SerializeInstallerRelaunchStarted(operation_id, child_pid)));
  };

  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalBuffered,
            correlator.Accept(terminal(kOperationId)));
  now += base::Seconds(1);
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kHandoffRecorded,
            correlator.Accept(handoff(kOtherOperationId, 2)));
  now += base::Seconds(1);
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalBuffered,
            correlator.Accept(terminal(kThirdOperationId)));
  EXPECT_EQ(2u, correlator.tracked_operation_count());

  // The oldest orphan terminal was evicted before the active handoff.
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kHandoffRecorded,
            correlator.Accept(handoff(kOperationId, 1)));
  EXPECT_FALSE(correlator.GetCorrelatedResult(kOperationId));
  EXPECT_EQ(2u, correlator.tracked_operation_count());
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kIgnored,
            correlator.Accept(terminal(kThirdOperationId)));
  EXPECT_EQ(2u, correlator.tracked_operation_count());

  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalCorrelated,
            correlator.Accept(terminal(kOperationId)));
  std::optional<ParsedInstallerLifecycleEvent> consumed =
      correlator.TakeCorrelatedResult(kOperationId);
  ASSERT_TRUE(consumed);
  EXPECT_EQ(kOperationId, consumed->operation_id);
  EXPECT_FALSE(correlator.GetCorrelatedResult(kOperationId));
  EXPECT_EQ(1u, correlator.tracked_operation_count());

  EXPECT_TRUE(correlator.Remove(kOtherOperationId));
  EXPECT_FALSE(correlator.Remove(kOtherOperationId));
  EXPECT_EQ(0u, correlator.tracked_operation_count());

  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalBuffered,
            correlator.Accept(terminal(kThirdOperationId)));
  EXPECT_TRUE(correlator.Remove(kThirdOperationId));
  EXPECT_EQ(0u, correlator.tracked_operation_count());
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kTerminalBuffered,
            correlator.Accept(terminal(kThirdOperationId)));
  now += base::Seconds(10);
  EXPECT_EQ(1u, correlator.PurgeExpired());
  EXPECT_EQ(0u, correlator.tracked_operation_count());
  EXPECT_EQ(InstallerLifecycleCorrelationStatus::kHandoffRecorded,
            correlator.Accept(handoff(kThirdOperationId, 3)));
}

TEST(InstallerLifecycleTest, SendIgnoresReceiverReturnValues) {
  const std::string json = *SerializeInstallerRelaunchStarted(kOperationId, 1);
  for (LRESULT receiver_result : {0, 1, 2, 999}) {
    LifecycleWindow window(receiver_result);
    ASSERT_TRUE(window.window());
    EXPECT_EQ(InstallerLifecycleSendStatus::kDelivered,
              SendInstallerLifecycleMessage(window.window(), json));
    EXPECT_EQ(1, window.messages());
    EXPECT_EQ(WithNul(json), window.payload());
  }
  EXPECT_EQ(InstallerLifecycleSendStatus::kInvalidWindow,
            SendInstallerLifecycleMessage(nullptr, json));
}

TEST(InstallerLifecycleTest, SendDestroyedAndHungWindowsIsBounded) {
  const std::string json = *SerializeInstallerRelaunchStarted(kOperationId, 1);
  HWND destroyed = nullptr;
  {
    LifecycleWindow window;
    ASSERT_TRUE(window.window());
    destroyed = window.window();
  }
  EXPECT_EQ(InstallerLifecycleSendStatus::kInvalidWindow,
            SendInstallerLifecycleMessage(destroyed, json));

  NonPumpingLifecycleWindow hung;
  ASSERT_TRUE(hung.window());
  const auto start = std::chrono::steady_clock::now();
  const InstallerLifecycleSendStatus status =
      SendInstallerLifecycleMessage(hung.window(), json);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  EXPECT_TRUE(status == InstallerLifecycleSendStatus::kTimeout ||
              status == InstallerLifecycleSendStatus::kError);
  EXPECT_LT(elapsed, std::chrono::seconds(2));
}

TEST(InstallerLifecycleTest, FinalizePreservesExitCodeAndSendsOnce) {
  LifecycleWindow window(2);
  ASSERT_TRUE(window.window());
  Result result = Result::Error(kExitCodeCancelled, "cancelled");
  UninstallLifecycleContext context{kOperationId, window.window()};
  EXPECT_EQ(kExitCodeCancelled, FinalizeUninstallLifecycle(context, result));
  EXPECT_EQ(1, window.messages());
  ParsedInstallerLifecycleEvent parsed =
      ParseInstallerLifecyclePayload(window.payload());
  ASSERT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
  EXPECT_EQ(kExitCodeCancelled, parsed.exit_code);
}

TEST(InstallerLifecycleTest, ControlledFailuresRouteExactlyOnce) {
  struct TestCase {
    ControlledUninstallFailure failure;
    int exit_code;
    const char* diagnostic;
  };
  const TestCase cases[] = {
      {ControlledUninstallFailure::kInvalidRetentionOptions,
       kExitCodeConfigError, "Invalid retention option usage"},
      {ControlledUninstallFailure::kInvalidRetentionAge, kExitCodeConfigError,
       "Invalid retention age"},
      {ControlledUninstallFailure::kConfigLoad, kExitCodeConfigError,
       "Failed to load installer configuration"},
      {ControlledUninstallFailure::kConfigNotFound, kExitCodeConfigError,
       "Installer configuration was not found"},
      {ControlledUninstallFailure::kExplicitModeDisabled, kExitCodeConfigError,
       "Explicit installer command is not enabled"},
      {ControlledUninstallFailure::kPreflightRejected, kExitCodePolicyError,
       "Relaunched uninstall preflight was rejected"},
  };
  for (const auto& test : cases) {
    LifecycleWindow window;
    ASSERT_TRUE(window.window());
    UninstallLifecycleContext context{kOperationId, window.window()};
    EXPECT_EQ(test.exit_code, FinalizeControlledUninstallFailure(
                                  context, test.failure, test.exit_code));
    EXPECT_EQ(1, window.messages());
    ParsedInstallerLifecycleEvent parsed =
        ParseInstallerLifecyclePayload(window.payload());
    ASSERT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
    ASSERT_TRUE(parsed.result);
    EXPECT_EQ(test.exit_code, parsed.exit_code);
    EXPECT_EQ(test.exit_code, parsed.result->error_code);
    EXPECT_EQ(test.diagnostic, parsed.result->error_message);
  }
}

TEST(InstallerLifecycleTest, ControllerFailuresRouteExactlyOnce) {
  for (const int exit_code :
       {kExitCodeDatabaseError, kExitCodeIndexError, kExitCodeLockTimeout,
        kExitCodePolicyError, kExitCodeInstallError, kExitCodeCancelled}) {
    LifecycleWindow window;
    ASSERT_TRUE(window.window());
    UninstallLifecycleContext context{kOperationId, window.window()};
    const Result result = Result::Error(exit_code, "controlled failure");
    EXPECT_EQ(exit_code, FinalizeUninstallLifecycle(context, result));
    EXPECT_EQ(1, window.messages());
    ParsedInstallerLifecycleEvent parsed =
        ParseInstallerLifecyclePayload(window.payload());
    ASSERT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
    ASSERT_TRUE(parsed.result);
    EXPECT_EQ(exit_code, parsed.exit_code);
    EXPECT_EQ(exit_code, parsed.result->error_code);
  }
}

TEST(InstallerLifecycleTest, TerminalFollowsProgressUiClose) {
  bool ui_closed = false;
  LifecycleWindow window(1, &ui_closed);
  ASSERT_TRUE(window.window());
  UninstallLifecycleContext context{kOperationId, window.window()};
  const Result result = Result::Success({}, "");
  EXPECT_EQ(kExitCodeSuccess,
            FinalizeUninstallLifecycleAfterProgressUi(
                context, result,
                base::BindOnce([](bool* closed) { *closed = true; },
                               base::Unretained(&ui_closed))));
  EXPECT_TRUE(ui_closed);
  EXPECT_EQ(1, window.messages());
  EXPECT_TRUE(window.prerequisite_observed());
}

}  // namespace
}  // namespace cef_installer
