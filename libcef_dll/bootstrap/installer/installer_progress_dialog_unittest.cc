// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"

#include <commctrl.h>

#include <atomic>
#include <optional>
#include <vector>

#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "cef/libcef_dll/bootstrap/installer/installer_resources.h"
#include "cef/libcef_dll/bootstrap/installer/installer_test_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
DWORD g_expected_resource_loader_thread = 0;
std::atomic<bool> g_resource_loaded_on_wrong_thread{false};
std::atomic<int> g_resource_load_count{0};
UINT g_resource_id_using_fallback = 0;
UINT g_resource_id_with_embedded_nul = 0;
const wchar_t* g_error_format_override = nullptr;

struct MessageBoxCall {
  int count = 0;
  HWND parent = nullptr;
  std::wstring message;
  std::wstring title;
  UINT flags = 0;
  WORD language_id = 0;
  DWORD thread_id = 0;
};

MessageBoxCall& CapturedMessageBoxCall() {
  static base::NoDestructor<MessageBoxCall> call;
  return *call;
}

void ResetMessageBoxCall() {
  CapturedMessageBoxCall() = {};
}

MessageBoxCall GetMessageBoxCall() {
  return CapturedMessageBoxCall();
}

int CaptureMessageBox(HWND parent,
                      const wchar_t* message,
                      const wchar_t* title,
                      UINT flags,
                      WORD language_id) {
  MessageBoxCall& call = CapturedMessageBoxCall();
  ++call.count;
  call.parent = parent;
  call.message = message;
  call.title = title;
  call.flags = flags;
  call.language_id = language_id;
  call.thread_id = GetCurrentThreadId();
  return IDOK;
}

using ThreadUiLanguages = std::vector<wchar_t>;

std::optional<ThreadUiLanguages> GetExplicitThreadUiLanguages() {
  constexpr DWORD kFlags = MUI_THREAD_LANGUAGES | MUI_LANGUAGE_NAME;
  ULONG language_count = 0;
  ULONG buffer_length = 0;
  if (!GetThreadPreferredUILanguages(kFlags, &language_count, nullptr,
                                     &buffer_length)) {
    return std::nullopt;
  }

  ThreadUiLanguages languages(buffer_length);
  if (buffer_length > 0 &&
      !GetThreadPreferredUILanguages(kFlags, &language_count, languages.data(),
                                     &buffer_length)) {
    return std::nullopt;
  }
  languages.resize(buffer_length);
  return languages;
}

bool SetExplicitThreadUiLanguages(const ThreadUiLanguages& languages) {
  // The literal contains two nulls: one explicit and one implicit.
  constexpr wchar_t kEmptyLanguageList[] = L"\0";
  ULONG language_count = 0;
  return SetThreadPreferredUILanguages(
      MUI_LANGUAGE_NAME,
      languages.empty() ? kEmptyLanguageList : languages.data(),
      &language_count);
}

class ScopedThreadUiLanguagesSnapshot {
 public:
  ScopedThreadUiLanguagesSnapshot()
      : original_languages_(GetExplicitThreadUiLanguages()) {}

  ~ScopedThreadUiLanguagesSnapshot() {
    if (original_languages_) {
      SetExplicitThreadUiLanguages(*original_languages_);
    }
  }

  bool captured() const { return original_languages_.has_value(); }

 private:
  const std::optional<ThreadUiLanguages> original_languages_;
};

class ScopedThreadUiLanguage {
 public:
  explicit ScopedThreadUiLanguage(LANGID language)
      : selected_(original_languages_.captured() &&
                  SetThreadUILanguage(language) == language) {}

  bool selected() const { return selected_; }

 private:
  const ScopedThreadUiLanguagesSnapshot original_languages_;
  const bool selected_;
};

std::wstring LoadDistinctTestString(UINT id, const wchar_t* default_value) {
  if (GetCurrentThreadId() != g_expected_resource_loader_thread) {
    g_resource_loaded_on_wrong_thread.store(true, std::memory_order_release);
  }
  g_resource_load_count.fetch_add(1, std::memory_order_relaxed);

  switch (id) {
    case IDS_INSTALLER_TITLE:
      return L"Test resource title";
    case IDS_INSTALLER_STEP_INITIALIZING:
      return L"Test resource initializing";
    case IDS_INSTALLER_STEP_CHECKING:
      return L"Test resource checking";
    case IDS_INSTALLER_STEP_DOWNLOADING:
      return L"Test resource downloading";
    case IDS_INSTALLER_STEP_EXTRACTING:
      return L"Test resource extracting";
    case IDS_INSTALLER_STEP_VERIFYING:
      return L"Test resource verifying";
    case IDS_INSTALLER_STEP_INSTALLING:
      return L"Test resource installing";
    case IDS_INSTALLER_STEP_COMMITTING:
      return L"Test resource committing";
    case IDS_INSTALLER_STEP_CLEANING:
      return L"Test resource cleaning";
    case IDS_INSTALLER_CANCEL:
      return L"Test resource cancel";
    case IDS_INSTALLER_ERROR_TITLE:
      return L"Test resource error title";
    case IDS_INSTALLER_ERROR_MESSAGE_FORMAT:
      return L"Test code $2 :: $1";
    case IDS_INSTALLER_ERROR_MESSAGE_CONFIG:
      return L"Test resource config";
    case IDS_INSTALLER_ERROR_MESSAGE_NETWORK:
      return L"Test resource network";
    case IDS_INSTALLER_ERROR_MESSAGE_VERIFICATION:
      return L"Test resource verification";
    case IDS_INSTALLER_ERROR_MESSAGE_COMPATIBILITY:
      return L"Test resource compatibility";
    case IDS_INSTALLER_ERROR_MESSAGE_BUSY:
      return L"Test resource busy";
    case IDS_INSTALLER_ERROR_MESSAGE_POLICY:
      return L"Test resource policy";
    case IDS_INSTALLER_ERROR_MESSAGE_GENERIC:
      return L"Test resource generic";
    case IDS_INSTALLER_PERCENT_FORMAT:
      return L"Test resource $1 percent";
  }
  return default_value;
}

std::wstring LoadTestStringWithFallback(UINT id, const wchar_t* default_value) {
  if (id == g_resource_id_using_fallback) {
    return default_value;
  }
  return LoadDistinctTestString(id, default_value);
}

std::wstring LoadTestStringWithFormatOverride(UINT id,
                                              const wchar_t* default_value) {
  if (id == IDS_INSTALLER_ERROR_MESSAGE_FORMAT && g_error_format_override) {
    return g_error_format_override;
  }
  return LoadDistinctTestString(id, default_value);
}

std::wstring LoadTestStringWithEmbeddedNul(UINT id,
                                           const wchar_t* default_value) {
  if (id == g_resource_id_with_embedded_nul) {
    if (id == IDS_INSTALLER_ERROR_MESSAGE_FORMAT) {
      return std::wstring(L"$1\0$2", 5);
    }
    return std::wstring(L"Truncated\0body", 14);
  }
  return LoadDistinctTestString(id, default_value);
}
#endif

std::wstring GetWindowTextForTesting(HWND hwnd) {
  wchar_t text[256] = {};
  const int length =
      GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
  return std::wstring(text, length);
}

// Helper to initialize common controls for tests
class CommonControlsInitializer {
 public:
  CommonControlsInitializer() {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
  }
};

class ProgressDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    internal::SetStringResourceLoaderForTesting(nullptr);
    internal::SetMessageBoxRunnerForTesting(nullptr);
    ResetMessageBoxCall();
    g_resource_id_using_fallback = 0;
    g_resource_id_with_embedded_nul = 0;
    g_error_format_override = nullptr;
#endif

    // Ensure common controls are initialized
    static CommonControlsInitializer initializer;

    // Check if dialog resource is available (it won't be in unit tests
    // unless we link the .rc file)
    HRSRC resource =
        FindResourceW(GetModuleHandle(nullptr),
                      MAKEINTRESOURCEW(IDD_INSTALLER_PROGRESS), RT_DIALOG);
    resource_available_ = (resource != nullptr);
  }

  void TearDown() override {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    internal::SetStringResourceLoaderForTesting(nullptr);
    internal::SetMessageBoxRunnerForTesting(nullptr);
#endif

    // Process any pending messages to clean up
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  bool ResourceAvailable() const { return resource_available_; }

  // Task environment for callbacks
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

 private:
  bool resource_available_ = false;
};

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(ProgressDialogTest, ScopedThreadUiLanguageRestoresPreferredLanguages) {
  ScopedThreadUiLanguagesSnapshot restore_original_languages;
  ASSERT_TRUE(restore_original_languages.captured());

  constexpr wchar_t kPreferredLanguages[] = L"en-US\0fr-FR\0";
  ULONG language_count = 0;
  ASSERT_TRUE(SetThreadPreferredUILanguages(
      MUI_LANGUAGE_NAME, kPreferredLanguages, &language_count));
  ASSERT_EQ(2u, language_count);

  const auto expected_languages = GetExplicitThreadUiLanguages();
  ASSERT_TRUE(expected_languages.has_value());
  {
    constexpr LANGID kTestLanguage = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN);
    ScopedThreadUiLanguage language(kTestLanguage);
    if (!language.selected()) {
      GTEST_SKIP() << "Host cannot select the test thread UI language";
    }
  }

  const auto restored_languages = GetExplicitThreadUiLanguages();
  ASSERT_TRUE(restored_languages.has_value());
  EXPECT_EQ(*expected_languages, *restored_languages);
}
#endif

TEST_F(ProgressDialogTest, CreateStandalone) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Before Show(), dialog thread hasn't started.
  ProgressDialog dialog;
  EXPECT_EQ(nullptr, dialog.GetHwnd());
  EXPECT_FALSE(dialog.WasCancelled());

  // Show() starts the thread and creates the window.
  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_NE(nullptr, dialog.GetHwnd());
  EXPECT_TRUE(dialog.IsVisible());
}

TEST_F(ProgressDialogTest, CreateWithParent) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Create a simple hidden parent window
  HWND parent =
      CreateWindowExW(0, L"STATIC", L"Parent", WS_OVERLAPPEDWINDOW, 0, 0, 100,
                      100, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  ASSERT_NE(nullptr, parent);

  {
    ProgressDialog dialog(parent);
    dialog.Show();
    EXPECT_NE(nullptr, dialog.GetHwnd());
  }

  DestroyWindow(parent);
}

TEST_F(ProgressDialogTest, ShowHide) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;

  // Show the dialog (starts thread and creates window)
  dialog.Show();
  dialog.FlushForTesting();
  ASSERT_NE(nullptr, dialog.GetHwnd());
  EXPECT_TRUE(dialog.IsVisible());

  // Hide the dialog
  dialog.Hide();
  dialog.FlushForTesting();
  EXPECT_FALSE(dialog.IsVisible());

  // Show again
  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.IsVisible());
}

TEST_F(ProgressDialogTest, SetStep) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  dialog.SetStep(kStepDownload);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  dialog.SetStep(kStepExtract);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  // Out-of-range step is ignored (no crash)
  dialog.SetStep(static_cast<Step>(-1));
  EXPECT_NE(nullptr, dialog.GetHwnd());
}

TEST_F(ProgressDialogTest, SetProgressDeterminate) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Set various progress values
  dialog.SetProgress(0);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  dialog.SetProgress(50);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  dialog.SetProgress(100);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  // Out of range values should be clamped
  dialog.SetProgress(-10);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  dialog.SetProgress(200);
  EXPECT_NE(nullptr, dialog.GetHwnd());
}

TEST_F(ProgressDialogTest, SetCancelEnabled) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Disable cancel
  dialog.SetCancelEnabled(false);
  EXPECT_NE(nullptr, dialog.GetHwnd());

  // Enable cancel
  dialog.SetCancelEnabled(true);
  EXPECT_NE(nullptr, dialog.GetHwnd());
}

TEST_F(ProgressDialogTest, CancelButtonSetsWasCancelled) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Simulate cancel button click by sending WM_COMMAND
  HWND cancel_button = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_CANCEL);
  if (cancel_button) {
    SendMessage(dialog.GetHwnd(), WM_COMMAND,
                MAKEWPARAM(IDC_INSTALLER_CANCEL, BN_CLICKED),
                reinterpret_cast<LPARAM>(cancel_button));
  } else {
    // Fallback: close also sets cancelled state
    SendMessage(dialog.GetHwnd(), WM_CLOSE, 0, 0);
  }

  EXPECT_TRUE(dialog.WasCancelled());
}

TEST_F(ProgressDialogTest,
       DeferredCancellationStartsEnabledThenDisablesAndRemainsPending) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());
  HWND cancel_button = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_CANCEL);
  ASSERT_NE(nullptr, cancel_button);

  dialog.SetCancellationDeferred(true);
  EXPECT_TRUE(IsWindowEnabled(cancel_button));
  SendMessage(dialog.GetHwnd(), WM_COMMAND,
              MAKEWPARAM(IDC_INSTALLER_CANCEL, BN_CLICKED),
              reinterpret_cast<LPARAM>(cancel_button));
  EXPECT_TRUE(dialog.WasCancelled());
  EXPECT_FALSE(IsWindowEnabled(cancel_button));
  // No queued transition task may re-enable the button after the request.
  dialog.FlushForTesting();
  EXPECT_FALSE(IsWindowEnabled(cancel_button));
  SendMessage(dialog.GetHwnd(), WM_CLOSE, 0, 0);
  EXPECT_TRUE(dialog.WasCancelled());
  EXPECT_TRUE(dialog.IsVisible());
  dialog.SetCancellationDeferred(false);
  EXPECT_TRUE(dialog.WasCancelled());
}

TEST_F(ProgressDialogTest,
       DeferredTransitionPreservesCancellationRacingBeforeEntry) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  dialog.FlushForTesting();
  ASSERT_NE(nullptr, dialog.GetHwnd());
  HWND cancel_button = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_CANCEL);
  ASSERT_NE(nullptr, cancel_button);

  SendMessage(dialog.GetHwnd(), WM_COMMAND,
              MAKEWPARAM(IDC_INSTALLER_CANCEL, BN_CLICKED),
              reinterpret_cast<LPARAM>(cancel_button));
  ASSERT_TRUE(dialog.WasCancelled());
  ASSERT_FALSE(IsWindowEnabled(cancel_button));

  dialog.SetCancellationDeferred(true);
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.WasCancelled());
  EXPECT_FALSE(IsWindowEnabled(cancel_button));
}

TEST_F(ProgressDialogTest, WasCancelledInitiallyFalse) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  EXPECT_FALSE(dialog.WasCancelled());
}

TEST_F(ProgressDialogTest, CloseProgram) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.IsVisible());

  // Close programmatically
  dialog.Close();

  // After close, hwnd should be null
  EXPECT_EQ(nullptr, dialog.GetHwnd());
}

TEST_F(ProgressDialogTest, AlwaysTopmost) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Dialog should always be topmost for visibility, regardless of parent
  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  LONG_PTR ex_style = GetWindowLongPtr(dialog.GetHwnd(), GWL_EXSTYLE);
  EXPECT_NE(0, ex_style & WS_EX_TOPMOST);
}

TEST_F(ProgressDialogTest, TopmostEvenWithParent) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Dialog should be topmost even when parent is specified.
  // We don't disable the parent (security: cross-process trust boundary).
  HWND parent = CreateWindowExW(0, L"STATIC", L"Parent", WS_OVERLAPPEDWINDOW,
                                100, 100, 200, 200, nullptr, nullptr,
                                GetModuleHandle(nullptr), nullptr);
  ASSERT_NE(nullptr, parent);
  ShowWindow(parent, SW_SHOW);

  {
    ProgressDialog dialog(parent);
    dialog.Show();
    ASSERT_NE(nullptr, dialog.GetHwnd());

    // Dialog is always topmost for visibility
    LONG_PTR ex_style = GetWindowLongPtr(dialog.GetHwnd(), GWL_EXSTYLE);
    EXPECT_NE(0, ex_style & WS_EX_TOPMOST);

    // Parent should NOT be disabled (security: we don't modify external
    // windows)
    EXPECT_TRUE(IsWindowEnabled(parent));
  }

  DestroyWindow(parent);
}

TEST_F(ProgressDialogTest, ParentNotDisabled) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Create a parent window
  HWND parent = CreateWindowExW(0, L"STATIC", L"Parent", WS_OVERLAPPEDWINDOW,
                                100, 100, 200, 200, nullptr, nullptr,
                                GetModuleHandle(nullptr), nullptr);
  ASSERT_NE(nullptr, parent);
  ShowWindow(parent, SW_SHOW);

  // Verify parent is initially enabled
  EXPECT_TRUE(IsWindowEnabled(parent));

  {
    ProgressDialog dialog(parent);
    dialog.Show();
    ASSERT_NE(nullptr, dialog.GetHwnd());

    // Parent should remain enabled - we don't modify external windows
    // (security: parent may be from a different process/trust level)
    EXPECT_TRUE(IsWindowEnabled(parent));
  }

  // Parent still enabled after dialog destroyed
  EXPECT_TRUE(IsWindowEnabled(parent));

  DestroyWindow(parent);
}

TEST_F(ProgressDialogTest, DestructorClosesDialog) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  HWND dialog_hwnd;
  {
    ProgressDialog dialog;
    dialog.Show();
    dialog_hwnd = dialog.GetHwnd();
    ASSERT_NE(nullptr, dialog_hwnd);
    EXPECT_TRUE(IsWindow(dialog_hwnd));
  }
  // After destructor, window should be destroyed
  EXPECT_FALSE(IsWindow(dialog_hwnd));
}

// ============================================================================
// Error and Success Dialog Tests
// ============================================================================

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(ProgressDialogTest, ShowErrorUsesCachedTextAndCallerLanguage) {
  constexpr LANGID kTestLanguage = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN);
  ScopedThreadUiLanguage language(kTestLanguage);
  if (!language.selected()) {
    GTEST_SKIP() << "Host cannot select the test thread UI language";
  }

  g_expected_resource_loader_thread = GetCurrentThreadId();
  g_resource_loaded_on_wrong_thread.store(false, std::memory_order_release);
  g_resource_load_count.store(0, std::memory_order_release);
  internal::SetStringResourceLoaderForTesting(&LoadDistinctTestString);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  ProgressDialog dialog;
  dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);

  const MessageBoxCall call = GetMessageBoxCall();
  EXPECT_EQ(1, call.count);
  EXPECT_EQ(nullptr, call.parent);
  EXPECT_EQ(L"Test code 103 :: Test resource compatibility", call.message);
  EXPECT_EQ(L"Test resource error title", call.title);
  EXPECT_EQ(
      static_cast<UINT>(MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND),
      call.flags);
  EXPECT_EQ(kTestLanguage, call.language_id);
  EXPECT_EQ(GetCurrentThreadId(), call.thread_id);
  EXPECT_FALSE(
      g_resource_loaded_on_wrong_thread.load(std::memory_order_acquire));
}

TEST_F(ProgressDialogTest, ShowErrorReturnsFromDialogThreadRunner) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  constexpr LANGID kTestLanguage = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN);
  ScopedThreadUiLanguage language(kTestLanguage);
  if (!language.selected()) {
    GTEST_SKIP() << "Host cannot select the test thread UI language";
  }

  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(&LoadDistinctTestString);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());
  dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);

  const MessageBoxCall call = GetMessageBoxCall();
  EXPECT_EQ(1, call.count);
  EXPECT_EQ(dialog.GetHwnd(), call.parent);
  EXPECT_EQ(L"Test code 103 :: Test resource compatibility", call.message);
  EXPECT_EQ(L"Test resource error title", call.title);
  EXPECT_EQ(static_cast<UINT>(MB_OK | MB_ICONERROR), call.flags);
  EXPECT_EQ(kTestLanguage, call.language_id);
  EXPECT_NE(GetCurrentThreadId(), call.thread_id);
}

TEST_F(ProgressDialogTest, CurrentErrorCodesMapToExpectedMessages) {
  struct TestCase {
    int error_code;
    const wchar_t* expected_body;
  };
  constexpr TestCase kTestCases[] = {
      {kExitCodeSuccess, L"Test resource generic"},
      {kExitCodeConfigError, L"Test resource config"},
      {kExitCodeNetworkError, L"Test resource network"},
      {kExitCodeSignatureError, L"Test resource verification"},
      {kExitCodeNoMatchingVersion, L"Test resource compatibility"},
      {kExitCodeExtractionError, L"Test resource generic"},
      {kExitCodeInstallError, L"Test resource generic"},
      {kExitCodeDatabaseError, L"Test resource generic"},
      {kExitCodeLockTimeout, L"Test resource busy"},
      {kExitCodeCancelled, L"Test resource generic"},
      {kExitCodeRelaunched, L"Test resource generic"},
      {kExitCodeNoSentinel, L"Test resource generic"},
      {kExitCodeSentinelReadError, L"Test resource generic"},
      {kExitCodeSentinelOwnerMismatch, L"Test resource generic"},
      {kExitCodePolicyDenied, L"Test resource policy"},
      {kExitCodeIndexError, L"Test resource generic"},
      {kExitCodeRecoveryError, L"Test resource generic"},
      {kExitCodeRepairError, L"Test resource generic"},
      {kExitCodeQuarantineError, L"Test resource generic"},
      {kExitCodeRetentionSnapshotChanged, L"Test resource generic"},
      {kExitCodePolicyError, L"Test resource policy"},
      {kExitCodeUnknownError, L"Test resource generic"},
  };

  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(&LoadDistinctTestString);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  ProgressDialog dialog;
  for (const auto& test : kTestCases) {
    ResetMessageBoxCall();
    dialog.ShowErrorDialog(test.error_code);
    const MessageBoxCall call = GetMessageBoxCall();
    EXPECT_EQ(1, call.count) << test.error_code;
    EXPECT_NE(std::wstring::npos, call.message.find(test.expected_body))
        << test.error_code;
  }
}

TEST_F(ProgressDialogTest, UnknownErrorCodeUsesGenericBodyAndPreservesCode) {
  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(&LoadDistinctTestString);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  ProgressDialog dialog;
  dialog.ShowErrorDialog(999);

  const MessageBoxCall call = GetMessageBoxCall();
  EXPECT_EQ(L"Test code 999 :: Test resource generic", call.message);
}

TEST_F(ProgressDialogTest, MissingErrorResourcesUseCompiledDefaults) {
  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(&LoadTestStringWithFallback);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  g_resource_id_using_fallback = IDS_INSTALLER_ERROR_MESSAGE_FORMAT;
  {
    ProgressDialog dialog;
    dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);
    EXPECT_EQ(L"Test resource compatibility\n\nError code: 103",
              GetMessageBoxCall().message);
  }

  ResetMessageBoxCall();
  g_resource_id_using_fallback = IDS_INSTALLER_ERROR_MESSAGE_COMPATIBILITY;
  {
    ProgressDialog dialog;
    dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);
    EXPECT_EQ(
        L"Test code 103 :: A compatible component version isn't "
        L"currently available. Check for an application update or "
        L"contact support.",
        GetMessageBoxCall().message);
  }
}

TEST_F(ProgressDialogTest, MalformedErrorFormatUsesCompleteDefault) {
  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(
      &LoadTestStringWithFormatOverride);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  constexpr const wchar_t* kMalformedFormats[] = {
      L"Only a body: $1",
      L"Only a code: $2",
  };
  for (const wchar_t* format : kMalformedFormats) {
    ResetMessageBoxCall();
    g_error_format_override = format;
    ProgressDialog dialog;
    dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);
    EXPECT_EQ(L"Test resource compatibility\n\nError code: 103",
              GetMessageBoxCall().message);
  }
}

TEST_F(ProgressDialogTest, EmbeddedNulErrorResourcesUseCompiledDefaults) {
  g_expected_resource_loader_thread = GetCurrentThreadId();
  internal::SetStringResourceLoaderForTesting(&LoadTestStringWithEmbeddedNul);
  internal::SetMessageBoxRunnerForTesting(&CaptureMessageBox);

  g_resource_id_with_embedded_nul = IDS_INSTALLER_ERROR_MESSAGE_FORMAT;
  {
    ProgressDialog dialog;
    dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);
    EXPECT_EQ(L"Test resource compatibility\n\nError code: 103",
              GetMessageBoxCall().message);
  }

  ResetMessageBoxCall();
  g_resource_id_with_embedded_nul = IDS_INSTALLER_ERROR_MESSAGE_COMPATIBILITY;
  {
    ProgressDialog dialog;
    dialog.ShowErrorDialog(kExitCodeNoMatchingVersion);
    EXPECT_EQ(
        L"Test code 103 :: A compatible component version isn't "
        L"currently available. Check for an application update or "
        L"contact support.",
        GetMessageBoxCall().message);
  }
}
#endif

// ============================================================================
// Localization Tests
// ============================================================================
// These tests verify that visible dialog chrome is loaded from string-table
// resources rather than left as text embedded in the dialog template.

TEST_F(ProgressDialogTest, SetStepAllSteps) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Exercise all valid steps
  for (int i = 0; i < kNumSteps; i++) {
    dialog.SetStep(static_cast<Step>(i));
    EXPECT_NE(nullptr, dialog.GetHwnd()) << "Failed at step " << i;
  }
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
TEST_F(ProgressDialogTest, ResourceOverridesDriveAllVisibleTextOnCallerThread) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  g_expected_resource_loader_thread = GetCurrentThreadId();
  g_resource_loaded_on_wrong_thread.store(false, std::memory_order_release);
  g_resource_load_count.store(0, std::memory_order_release);
  internal::SetStringResourceLoaderForTesting(&LoadDistinctTestString);

  ProgressDialog dialog;
  EXPECT_EQ(22, g_resource_load_count.load(std::memory_order_acquire));
  EXPECT_FALSE(
      g_resource_loaded_on_wrong_thread.load(std::memory_order_acquire));

  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  EXPECT_EQ(L"Test resource title", GetWindowTextForTesting(dialog.GetHwnd()));
  HWND cancel = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_CANCEL);
  ASSERT_NE(nullptr, cancel);
  EXPECT_EQ(L"Test resource cancel", GetWindowTextForTesting(cancel));

  HWND step = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_STEP);
  ASSERT_NE(nullptr, step);
  EXPECT_EQ(L"Test resource initializing", GetWindowTextForTesting(step));
  dialog.SetStep(kStepDownload);

  HWND percent = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  ASSERT_NE(nullptr, percent);
  dialog.SetProgress(37);
  dialog.FlushForTesting();
  EXPECT_EQ(L"Test resource downloading", GetWindowTextForTesting(step));
  EXPECT_EQ(L"Test resource 37 percent", GetWindowTextForTesting(percent));
  EXPECT_EQ(22, g_resource_load_count.load(std::memory_order_acquire));
  EXPECT_FALSE(
      g_resource_loaded_on_wrong_thread.load(std::memory_order_acquire));
}
#endif

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(ProgressDialogTest, MultipleShowCalls) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Multiple show calls should not crash
  dialog.Show();
  dialog.Show();
  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.IsVisible());
}

TEST_F(ProgressDialogTest, MultipleHideCalls) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  dialog.Show();
  dialog.FlushForTesting();
  EXPECT_TRUE(dialog.IsVisible());

  // Multiple hide calls should not crash
  dialog.Hide();
  dialog.Hide();
  dialog.Hide();
  dialog.FlushForTesting();
  EXPECT_FALSE(dialog.IsVisible());
}

TEST_F(ProgressDialogTest, RapidProgressUpdates) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  // Rapid progress updates should not crash or cause issues
  for (int i = 0; i <= 100; i++) {
    dialog.SetProgress(i);
  }
  EXPECT_NE(nullptr, dialog.GetHwnd());
}

// ============================================================================
// LoadStringResource Tests
// ============================================================================

TEST_F(ProgressDialogTest, LoadStringResource_ReturnsDefault) {
  // When resource is not found, should return the default value
  constexpr wchar_t kDefault[] = L"Default Value";
  // Use a resource ID that definitely doesn't exist
  std::wstring result = internal::LoadStringResource(99999, kDefault);
  EXPECT_EQ(kDefault, result);
}

TEST_F(ProgressDialogTest, LoadStringResource_EmptyDefault) {
  // Test with empty default
  std::wstring result = internal::LoadStringResource(99999, L"");
  EXPECT_TRUE(result.empty());
}

TEST_F(ProgressDialogTest, LoadStringResource_DoesNotTruncateLongResource) {
  const std::wstring result =
      internal::LoadStringResource(IDS_INSTALLER_TEST_LONG_STRING, L"fallback");
  ASSERT_GT(result.size(), 255u);
  EXPECT_EQ(L"-END", result.substr(result.size() - 4));
}

// ============================================================================
// Resource Fallback Tests
// ============================================================================

TEST_F(ProgressDialogTest, DialogCreationFailsGracefully) {
  // This test verifies that when the dialog resource is not available,
  // Show() handles it gracefully (returns null hwnd) rather than crashing.
  ProgressDialog dialog;
  dialog.Show();

  if (!ResourceAvailable()) {
    // Without the resource, dialog creation should fail gracefully
    EXPECT_EQ(nullptr, dialog.GetHwnd());
  } else {
    // With the resource, dialog should be created
    EXPECT_NE(nullptr, dialog.GetHwnd());
  }
}

TEST_F(ProgressDialogTest, InvalidParentHandledGracefully) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Create a parent window and then destroy it before creating the dialog
  HWND parent =
      CreateWindowExW(0, L"STATIC", L"Parent", WS_OVERLAPPEDWINDOW, 0, 0, 100,
                      100, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  ASSERT_NE(nullptr, parent);

  // Destroy the parent - this makes the HWND invalid
  DestroyWindow(parent);

  // Now create dialog with the invalid parent - should not crash
  // The dialog should handle this gracefully
  ProgressDialog dialog(parent);
  dialog.Show();

  // Dialog should still be created (centered on screen instead of parent)
  EXPECT_NE(nullptr, dialog.GetHwnd());
}

TEST_F(ProgressDialogTest, ParentDestroyedDialogSurvives) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  // Create a parent window (used only for positioning)
  HWND parent = CreateWindowExW(0, L"STATIC", L"Parent", WS_OVERLAPPEDWINDOW,
                                100, 100, 200, 200, nullptr, nullptr,
                                GetModuleHandle(nullptr), nullptr);
  ASSERT_NE(nullptr, parent);
  ShowWindow(parent, SW_SHOW);

  {
    ProgressDialog dialog(parent);
    dialog.Show();
    ASSERT_NE(nullptr, dialog.GetHwnd());
    HWND dialog_hwnd = dialog.GetHwnd();

    // Destroy the parent while dialog exists.
    // Dialog should survive because we don't establish an owner relationship
    // (security: parent HWND is only used for positioning, not ownership).
    DestroyWindow(parent);

    // Dialog should still be valid - no owner relationship
    EXPECT_TRUE(IsWindow(dialog_hwnd));
    EXPECT_NE(nullptr, dialog.GetHwnd());

    // Close works normally
    dialog.Close();
    EXPECT_EQ(nullptr, dialog.GetHwnd());
  }
}

// ============================================================================
// Overall Percent Label Tests
// ============================================================================

TEST_F(ProgressDialogTest, PercentLabelExists) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  HWND percent_label = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  EXPECT_NE(nullptr, percent_label);
}

TEST_F(ProgressDialogTest, PercentLabelInitialValue) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  HWND percent_label = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  ASSERT_NE(nullptr, percent_label);

  wchar_t buf[16] = {};
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"0%", buf);
}

TEST_F(ProgressDialogTest, PercentLabelUpdatesWithProgress) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  HWND percent_label = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  ASSERT_NE(nullptr, percent_label);

  dialog.SetProgress(42);
  dialog.FlushForTesting();

  wchar_t buf[16] = {};
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"42%", buf);
}

TEST_F(ProgressDialogTest, PercentLabelAtBoundaries) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  HWND percent_label = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  ASSERT_NE(nullptr, percent_label);

  wchar_t buf[16] = {};

  dialog.SetProgress(0);
  dialog.FlushForTesting();
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"0%", buf);

  dialog.SetProgress(100);
  dialog.FlushForTesting();
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"100%", buf);
}

TEST_F(ProgressDialogTest, PercentLabelClampsOutOfRange) {
  if (!ResourceAvailable()) {
    GTEST_SKIP() << "Dialog resource not available in test executable";
  }

  ProgressDialog dialog;
  dialog.Show();
  ASSERT_NE(nullptr, dialog.GetHwnd());

  HWND percent_label = GetDlgItem(dialog.GetHwnd(), IDC_INSTALLER_PERCENT);
  ASSERT_NE(nullptr, percent_label);

  wchar_t buf[16] = {};

  dialog.SetProgress(-10);
  dialog.FlushForTesting();
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"0%", buf);

  dialog.SetProgress(200);
  dialog.FlushForTesting();
  GetWindowTextW(percent_label, buf, static_cast<int>(std::size(buf)));
  EXPECT_STREQ(L"100%", buf);
}

}  // namespace
}  // namespace cef_installer
