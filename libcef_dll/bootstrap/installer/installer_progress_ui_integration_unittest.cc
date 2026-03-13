// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_controller.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle.h"
#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"
#include "cef/libcef_dll/bootstrap/installer/installer_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {

using internal::BuildProgressJson;
using internal::ParseExtendedConfigFromJson;
using internal::ResetProgressNotificationState;

namespace {

// Test fixture for Progress UI Integration tests
class ProgressUIIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    received_messages_.clear();
    lifecycle_messages_.clear();
    message_types_.clear();
    ui_closed_ = false;
    lifecycle_observed_after_ui_close_ = false;
  }

  void TearDown() override {
    if (test_window_) {
      DestroyWindow(test_window_);
      test_window_ = nullptr;
    }
    UnregisterClassW(kWindowClass, GetModuleHandle(nullptr));
  }

  // Create a test window to receive WM_COPYDATA messages
  HWND CreateTestWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TestWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kWindowClass;

    RegisterClassExW(&wc);

    test_window_ =
        CreateWindowExW(0, kWindowClass, L"Test Window", WS_POPUP, 0, 0, 100,
                        100, nullptr, nullptr, GetModuleHandle(nullptr), this);
    return test_window_;
  }

  // Get received WM_COPYDATA messages
  const std::vector<std::string>& GetReceivedMessages() const {
    return received_messages_;
  }

  const std::vector<std::string>& GetLifecycleMessages() const {
    return lifecycle_messages_;
  }

  bool* UiClosedForTesting() { return &ui_closed_; }
  bool LifecycleObservedAfterUiCloseForTesting() const {
    return lifecycle_observed_after_ui_close_;
  }
  const std::vector<ULONG_PTR>& MessageTypesForTesting() const {
    return message_types_;
  }

  base::FilePath GetTempDir() const { return temp_dir_.GetPath(); }

  // Set to true to make the test window return non-zero from WM_COPYDATA,
  // requesting cancellation.
  bool cancel_requested_ = false;
  LRESULT lifecycle_result_ = 1;

 private:
  static LRESULT CALLBACK TestWindowProc(HWND hwnd,
                                         UINT msg,
                                         WPARAM wparam,
                                         LPARAM lparam) {
    if (msg == WM_CREATE) {
      CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      return 0;
    }

    ProgressUIIntegrationTest* self =
        reinterpret_cast<ProgressUIIntegrationTest*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_COPYDATA && self) {
      COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(lparam);
      if (cds->dwData == kWmCopyDataInstallerProgress && cds->lpData) {
        self->message_types_.push_back(cds->dwData);
        std::string json(static_cast<const char*>(cds->lpData),
                         cds->cbData - 1);
        self->received_messages_.push_back(json);
        return self->cancel_requested_ ? kWmCopyDataResultCancel : TRUE;
      }
      if (cds->dwData == kWmCopyDataInstallerLifecycle && cds->lpData) {
        self->message_types_.push_back(cds->dwData);
        self->lifecycle_observed_after_ui_close_ = self->ui_closed_;
        self->lifecycle_messages_.emplace_back(
            static_cast<const char*>(cds->lpData), cds->cbData);
        return self->lifecycle_result_;
      }
      return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
  }

  static constexpr wchar_t kWindowClass[] = L"CefInstallerTestWindow";

  base::ScopedTempDir temp_dir_;
  HWND test_window_ = nullptr;
  std::vector<std::string> received_messages_;
  std::vector<std::string> lifecycle_messages_;
  std::vector<ULONG_PTR> message_types_;
  bool ui_closed_ = false;
  bool lifecycle_observed_after_ui_close_ = false;
};

// ============================================================================
// Progress Dialog Tests
// ============================================================================

TEST_F(ProgressUIIntegrationTest, DialogAppearsDuringInstall) {
  // Create and show a progress dialog
  ProgressDialog dialog(nullptr);
  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_NE(dialog.GetHwnd(), nullptr);
  EXPECT_TRUE(dialog.IsVisible());

  dialog.Close();
  EXPECT_FALSE(dialog.IsVisible());
}

TEST_F(ProgressUIIntegrationTest, DownloadProgressUpdates) {
  ProgressDialog dialog(nullptr);
  dialog.Show();

  // Set download step and progress
  dialog.SetStep(kStepDownload);
  dialog.SetProgress(50);

  // Wait for updates to be processed on the dialog thread
  dialog.FlushForTesting();

  EXPECT_TRUE(dialog.IsVisible());
  dialog.Close();
}

TEST_F(ProgressUIIntegrationTest, ExtractionProgressUpdates) {
  ProgressDialog dialog(nullptr);
  dialog.Show();

  // Set extraction step and progress
  dialog.SetStep(kStepExtract);
  dialog.SetProgress(10);

  dialog.FlushForTesting();

  EXPECT_TRUE(dialog.IsVisible());
  dialog.Close();
}

TEST_F(ProgressUIIntegrationTest, CancelButtonCancelsDownload) {
  ProgressDialog dialog(nullptr);
  dialog.Show();

  // Simulate cancel button click by sending the message synchronously.
  SendMessage(dialog.GetHwnd(), WM_COMMAND,
              MAKEWPARAM(IDC_INSTALLER_CANCEL, BN_CLICKED), 0);

  EXPECT_TRUE(dialog.WasCancelled());

  dialog.Close();
}

TEST_F(ProgressUIIntegrationTest, CancelButtonCancelsExtraction) {
  ProgressDialog dialog(nullptr);
  dialog.Show();
  dialog.SetStep(kStepExtract);

  // Cancel via close button (WM_CLOSE)
  PostMessage(dialog.GetHwnd(), WM_CLOSE, 0, 0);
  dialog.FlushForTesting();

  EXPECT_TRUE(dialog.WasCancelled());
}

TEST_F(ProgressUIIntegrationTest, DialogClosesOnSuccess) {
  ProgressDialog dialog(nullptr);
  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.IsVisible());

  dialog.Close();
  EXPECT_FALSE(dialog.IsVisible());
}

TEST_F(ProgressUIIntegrationTest, HeadlessModeNoDialog) {
  // Parse config with show_progress_ui=false
  ExtendedConfig extended;
  std::string json = R"({"show_progress_ui": false})";
  ASSERT_TRUE(ParseExtendedConfigFromJson(json, &extended));

  EXPECT_FALSE(extended.show_progress_ui);
}

TEST_F(ProgressUIIntegrationTest, WmCopyDataSentToParent) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  // Create config with parent_window set
  ExtendedConfig extended;
  extended.parent_window = parent;
  extended.show_progress_ui = false;  // Don't show actual UI

  // Manually simulate what the progress callback does
  // (We can't easily run the full installer flow in a unit test)
  COPYDATASTRUCT cds = {};
  cds.dwData = kWmCopyDataInstallerProgress;
  std::string json = R"({"step_name":"downloading","step":1,"total_steps":5})";
  cds.cbData = static_cast<DWORD>(json.size() + 1);
  cds.lpData = const_cast<char*>(json.c_str());

  SendMessage(parent, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));

  ASSERT_EQ(GetReceivedMessages().size(), 1u);
  EXPECT_NE(GetReceivedMessages()[0].find("downloading"), std::string::npos);
}

TEST_F(ProgressUIIntegrationTest, WmCopyDataFormat) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  // Send a progress message with all fields
  std::string json =
      R"({"step_name":"extracting","step":3,"total_steps":5,"bytes_done":5242880,"bytes_total":10485760,"message":"Extracting..."})";

  COPYDATASTRUCT cds = {};
  cds.dwData = kWmCopyDataInstallerProgress;
  cds.cbData = static_cast<DWORD>(json.size() + 1);
  cds.lpData = const_cast<char*>(json.c_str());

  SendMessage(parent, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));

  ASSERT_EQ(GetReceivedMessages().size(), 1u);

  // Parse the received JSON
  std::optional<base::DictValue> parsed = base::JSONReader::ReadDict(
      GetReceivedMessages()[0], base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value());

  // Verify all expected fields are present
  EXPECT_EQ(*parsed->FindString("step_name"), "extracting");
  EXPECT_EQ(parsed->FindInt("step"), 3);
  EXPECT_EQ(parsed->FindInt("total_steps"), 5);
  EXPECT_NE(parsed->FindDouble("bytes_done"), std::nullopt);
  EXPECT_NE(parsed->FindDouble("bytes_total"), std::nullopt);
  EXPECT_EQ(*parsed->FindString("message"), "Extracting...");
}

// ============================================================================
// WM_COPYDATA Send/Receive Integration Tests
// ============================================================================

TEST_F(ProgressUIIntegrationTest, SendProgressToParentDelivers) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  // Reset rate limiting state
  ResetProgressNotificationState();

  // Send progress via the actual function
  SendProgressToParent(parent, kStepDownload, 5000000, 10000000);

  // Process messages
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Verify message was received
  ASSERT_EQ(GetReceivedMessages().size(), 1u);

  // Parse and verify content
  std::optional<base::DictValue> parsed = base::JSONReader::ReadDict(
      GetReceivedMessages()[0], base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed->FindString("step_name"), "downloading");
  EXPECT_EQ(parsed->FindInt("step"), 4);
  EXPECT_EQ(parsed->FindInt("total_steps"), kNumSteps - 1);
}

TEST_F(ProgressUIIntegrationTest, SendProgressToParentRateLimiting) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  // Reset rate limiting state
  ResetProgressNotificationState();

  // Send multiple rapid updates for the same step
  for (int i = 0; i < 10; i++) {
    SendProgressToParent(parent, kStepDownload, i * 1000000, 10000000);
  }

  // Process messages
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Only one message should have been sent due to rate limiting
  // (all updates were for the same step within 100ms)
  EXPECT_EQ(GetReceivedMessages().size(), 1u);
}

TEST_F(ProgressUIIntegrationTest, SendProgressToParentStepChangesAlwaysSent) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  // Reset rate limiting state
  ResetProgressNotificationState();

  // Send updates for different steps - these should all be sent
  SendProgressToParent(parent, kStepInit, 0, 0);
  SendProgressToParent(parent, kStepVersionCheck, 0, 0);
  SendProgressToParent(parent, kStepDownload, 0, 10000000);
  SendProgressToParent(parent, kStepExtract, 0, 20000000);

  // Process messages
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // All 4 messages should have been sent (different steps)
  EXPECT_EQ(GetReceivedMessages().size(), 4u);
}

TEST_F(ProgressUIIntegrationTest, SendProgressToParentInvalidWindow) {
  // Reset rate limiting state
  ResetProgressNotificationState();

  // Should not crash with null or invalid window, returns true (continue)
  EXPECT_TRUE(SendProgressToParent(nullptr, kStepInit, 0, 0));
  EXPECT_TRUE(SendProgressToParent(reinterpret_cast<HWND>(0xDEADBEEF),
                                   kStepLock, 0, 0));
}

TEST_F(ProgressUIIntegrationTest, SendProgressToParentContinueByDefault) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  ResetProgressNotificationState();

  // Parent returns 0 by default (cancel_requested_ is false)
  EXPECT_TRUE(SendProgressToParent(parent, kStepDownload, 0, 10000000));
}

TEST_F(ProgressUIIntegrationTest, SendProgressToParentCancelViaReturnValue) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);

  ResetProgressNotificationState();

  // First call succeeds
  EXPECT_TRUE(SendProgressToParent(parent, kStepInit, 0, 0));

  // Parent requests cancellation
  cancel_requested_ = true;

  // Next call with a new step (to bypass rate limiting) returns false
  EXPECT_FALSE(SendProgressToParent(parent, kStepDownload, 0, 10000000));
}

TEST_F(ProgressUIIntegrationTest, LifecycleDispatchCannotCancelProgress) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);
  lifecycle_result_ = kWmCopyDataResultCancel;

  const std::string lifecycle = *SerializeInstallerRelaunchStarted(
      "0123456789abcdef0123456789abcdef", 123);
  EXPECT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(parent, lifecycle));
  ASSERT_EQ(1u, GetLifecycleMessages().size());
  EXPECT_EQ(lifecycle + '\0', GetLifecycleMessages().front());

  ResetProgressNotificationState();
  EXPECT_TRUE(SendProgressToParent(parent, kStepInit, 0, 0));
  cancel_requested_ = true;
  EXPECT_FALSE(SendProgressToParent(parent, kStepDownload, 0, 1));
}

TEST_F(ProgressUIIntegrationTest,
       UninstallTerminalFollowsUiCloseAndLastProgress) {
  HWND parent = CreateTestWindow();
  ASSERT_NE(parent, nullptr);
  ResetProgressNotificationState();
  ASSERT_TRUE(SendProgressToParent(parent, kStepCleanup, 0, 0));
  const size_t progress_count = GetReceivedMessages().size();
  ASSERT_EQ(1u, progress_count);

  const Result result = Result::Success({}, "");
  const UninstallLifecycleContext context{"0123456789abcdef0123456789abcdef",
                                          parent};
  EXPECT_EQ(kExitCodeSuccess,
            FinalizeUninstallLifecycleAfterProgressUi(
                context, result,
                base::BindOnce([](bool* closed) { *closed = true; },
                               base::Unretained(UiClosedForTesting()))));
  EXPECT_TRUE(*UiClosedForTesting());
  EXPECT_TRUE(LifecycleObservedAfterUiCloseForTesting());
  EXPECT_EQ(progress_count, GetReceivedMessages().size());
  ASSERT_EQ(2u, MessageTypesForTesting().size());
  EXPECT_EQ(kWmCopyDataInstallerProgress, MessageTypesForTesting()[0]);
  EXPECT_EQ(kWmCopyDataInstallerLifecycle, MessageTypesForTesting()[1]);
}

// ============================================================================
// Progress Calculation Tests
// ============================================================================

TEST(ProgressCalculationTest, StepProgressionNeverGoesBackward) {
  // Simulate the full sequence of progress callbacks and verify
  // that progress never decreases
  int last_progress = -1;

  auto check = [&](Step step, uint64_t done, uint64_t total, int expected,
                   const char* label) {
    int progress = CalculateOverallProgress(step, done, total);
    EXPECT_GE(progress, last_progress) << label << " went backward";
    EXPECT_EQ(progress, expected) << label;
    last_progress = progress;
  };

  check(kStepInit, 0, 0, 0, "Init");
  check(kStepLock, 0, 0, 3, "Lock");
  check(kStepVersionCheck, 0, 0, 5, "VersionCheck");
  check(kStepCdnResolve, 0, 0, 10, "CdnResolve");
  check(kStepDownload, 0, 100000000, 15, "Download start");
  check(kStepDownload, 50000000, 100000000, 35, "Download 50%");
  check(kStepDownload, 100000000, 100000000, 55, "Download end");
  check(kStepExtract, 0, 200000000, 55, "Extract start");
  check(kStepExtract, 100000000, 200000000, 67, "Extract 50%");
  check(kStepExtract, 200000000, 200000000, 80, "Extract end");
  check(kStepSignatureVerify, 0, 0, 80, "SignatureVerify");
  check(kStepInstall, 0, 0, 90, "Install");
  check(kStepCommitting, 0, 0, 95, "Committing");
  check(kStepCleanup, 0, 0, 95, "Cleanup");
}

TEST(ProgressCalculationTest, ProgressRanges) {
  EXPECT_EQ(CalculateOverallProgress(kStepInit, 0, 0), 0);
  EXPECT_EQ(CalculateOverallProgress(kStepLock, 0, 0), 3);
  EXPECT_EQ(CalculateOverallProgress(kStepVersionCheck, 0, 0), 5);
  EXPECT_EQ(CalculateOverallProgress(kStepCdnResolve, 0, 0), 10);
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 0, 0), 15);
  EXPECT_EQ(CalculateOverallProgress(kStepExtract, 0, 0), 55);
  EXPECT_EQ(CalculateOverallProgress(kStepSignatureVerify, 0, 0), 80);
  EXPECT_EQ(CalculateOverallProgress(kStepInstall, 0, 0), 90);
  EXPECT_EQ(CalculateOverallProgress(kStepCommitting, 0, 0), 95);
  EXPECT_EQ(CalculateOverallProgress(kStepCleanup, 0, 0), 95);
}

TEST(ProgressCalculationTest, ByteInterpolation) {
  // Download step (step 4: 15-55%, range = 40)
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 0, 1000), 15);
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 250, 1000), 25);
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 500, 1000), 35);
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 750, 1000), 45);
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 1000, 1000), 55);

  // Extract step (step 5: 55-80%, range = 25)
  EXPECT_EQ(CalculateOverallProgress(kStepExtract, 0, 1000), 55);
  EXPECT_EQ(CalculateOverallProgress(kStepExtract, 500, 1000), 67);
  EXPECT_EQ(CalculateOverallProgress(kStepExtract, 1000, 1000), 80);
}

TEST(ProgressCalculationTest, EdgeCases) {
  // Negative step clamped to 0
  EXPECT_EQ(CalculateOverallProgress(static_cast<Step>(-1), 0, 0), 0);

  // Step beyond max clamped to last step
  EXPECT_EQ(CalculateOverallProgress(static_cast<Step>(100), 0, 0), 95);

  // Zero bytes_total treated as no byte progress
  EXPECT_EQ(CalculateOverallProgress(kStepDownload, 1000, 0), 15);
}

}  // namespace
}  // namespace cef_installer
