// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_progress_dialog.h"

#include <commctrl.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "cef/libcef_dll/bootstrap/installer/installer_resources.h"

// Link required libraries
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")

namespace cef_installer {

namespace {

// Initialize common controls (required for progress bar)
void InitCommonControls() {
  static bool initialized = false;
  if (!initialized) {
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
    initialized = true;
  }
}

// Resource IDs for each step, indexed by Step.
constexpr UINT kStepResourceIds[] = {
    IDS_INSTALLER_STEP_INITIALIZING,  // kStepInit
    IDS_INSTALLER_STEP_INITIALIZING,  // kStepLock
    IDS_INSTALLER_STEP_CHECKING,      // kStepVersionCheck
    IDS_INSTALLER_STEP_CHECKING,      // kStepCdnResolve
    IDS_INSTALLER_STEP_DOWNLOADING,   // kStepDownload
    IDS_INSTALLER_STEP_EXTRACTING,    // kStepExtract
    IDS_INSTALLER_STEP_VERIFYING,     // kStepSignatureVerify
    IDS_INSTALLER_STEP_INSTALLING,    // kStepInstall
    IDS_INSTALLER_STEP_COMMITTING,    // kStepCommitting
    IDS_INSTALLER_STEP_CLEANING,      // kStepCleanup
};

static_assert(std::size(kStepResourceIds) == kNumSteps,
              "kStepResourceIds size mismatch");

enum class ErrorMessageKind {
  kConfig,
  kNetwork,
  kVerification,
  kCompatibility,
  kBusy,
  kPolicy,
  kGeneric,
  kCount,
};

struct ErrorMessageResource {
  UINT id;
  const wchar_t* default_value;
};

constexpr ErrorMessageResource kErrorMessageResources[] = {
    {IDS_INSTALLER_ERROR_MESSAGE_CONFIG,
     L"The application isn't configured correctly. Check for an application "
     L"update or contact support."},
    {IDS_INSTALLER_ERROR_MESSAGE_NETWORK,
     L"The files needed for installation couldn't be downloaded. Check your "
     L"internet connection and try again."},
    {IDS_INSTALLER_ERROR_MESSAGE_VERIFICATION,
     L"The downloaded files couldn't be verified. Try again. If the problem "
     L"continues, contact support."},
    {IDS_INSTALLER_ERROR_MESSAGE_COMPATIBILITY,
     L"A compatible component version isn't currently available. Check for "
     L"an application update or contact support."},
    {IDS_INSTALLER_ERROR_MESSAGE_BUSY,
     L"Another setup operation may be in progress. Wait for it to finish, "
     L"then try again."},
    {IDS_INSTALLER_ERROR_MESSAGE_POLICY,
     L"The requested operation couldn't continue because of an organization "
     L"policy. Contact your system administrator."},
    {IDS_INSTALLER_ERROR_MESSAGE_GENERIC,
     L"The requested operation couldn't be completed. Try again. If the "
     L"problem continues, contact support."},
};

static_assert(std::size(kErrorMessageResources) ==
              static_cast<size_t>(ErrorMessageKind::kCount));

constexpr wchar_t kDefaultErrorMessageFormat[] = L"$1\n\nError code: $2";

ErrorMessageKind ErrorCodeToMessageKind(int error_code) {
  switch (error_code) {
    case kExitCodeConfigError:
      return ErrorMessageKind::kConfig;
    case kExitCodeNetworkError:
      return ErrorMessageKind::kNetwork;
    case kExitCodeSignatureError:
      return ErrorMessageKind::kVerification;
    case kExitCodeNoMatchingVersion:
      return ErrorMessageKind::kCompatibility;
    case kExitCodeLockTimeout:
      return ErrorMessageKind::kBusy;
    case kExitCodePolicyDenied:
    case kExitCodePolicyError:
      return ErrorMessageKind::kPolicy;
    case kExitCodeExtractionError:
    case kExitCodeInstallError:
    case kExitCodeDatabaseError:
    case kExitCodeIndexError:
    case kExitCodeRecoveryError:
    case kExitCodeRepairError:
    case kExitCodeQuarantineError:
    case kExitCodeRetentionSnapshotChanged:
    case kExitCodeSuccess:
    case kExitCodeCancelled:
    case kExitCodeRelaunched:
    case kExitCodeNoSentinel:
    case kExitCodeSentinelReadError:
    case kExitCodeSentinelOwnerMismatch:
    case kExitCodeUnknownError:
      return ErrorMessageKind::kGeneric;
    default:
      return ErrorMessageKind::kGeneric;
  }
}

bool HasUnescapedPlaceholder(const std::wstring& format,
                             wchar_t placeholder_number) {
  const wchar_t placeholder[] = {L'$', placeholder_number, L'\0'};
  size_t position = 0;
  while ((position = format.find(placeholder, position)) !=
         std::wstring::npos) {
    if (position == 0 || format[position - 1] != L'$') {
      return true;
    }
    position += 2;
  }
  return false;
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
std::atomic<internal::MessageBoxRunner> g_message_box_runner_for_tests{nullptr};
std::atomic<internal::StringResourceLoader> g_string_resource_loader_for_tests{
    nullptr};
#endif

void ShowErrorMessageBox(HWND parent,
                         const std::wstring& title,
                         const std::wstring& message,
                         LANGID language_id) {
  UINT flags = MB_OK | MB_ICONERROR;
  if (!parent) {
    flags |= MB_TOPMOST | MB_SETFOREGROUND;
  }
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (internal::MessageBoxRunner runner =
          g_message_box_runner_for_tests.load(std::memory_order_acquire)) {
    runner(parent, message.c_str(), title.c_str(), flags, language_id);
    return;
  }
#endif
  MessageBoxExW(parent, message.c_str(), title.c_str(), flags, language_id);
}

// The single active ProgressDialog instance, if any.
ProgressDialog* g_current_dialog = nullptr;

}  // namespace

namespace internal {

// Load a string resource with fallback to default.
std::wstring LoadStringResource(UINT id, const wchar_t* default_value) {
  std::wstring value;
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (StringResourceLoader loader =
          g_string_resource_loader_for_tests.load(std::memory_order_acquire)) {
    value = loader(id, default_value);
  } else {
#endif
    wchar_t* resource_string = nullptr;
    const int len =
        LoadStringW(GetModuleHandle(nullptr), id,
                    reinterpret_cast<wchar_t*>(&resource_string), 0);
    if (len <= 0 || !resource_string) {
      return default_value;
    }
    value.assign(resource_string, len);
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  }
#endif

  if (value.find(L'\0') != std::wstring::npos) {
    LOG(WARNING) << "String resource " << id
                 << " contains an embedded NUL; using the default";
    return default_value;
  }
  return value;
}

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
void SetStringResourceLoaderForTesting(StringResourceLoader loader) {
  g_string_resource_loader_for_tests.store(loader, std::memory_order_release);
}

void SetMessageBoxRunnerForTesting(MessageBoxRunner runner) {
  g_message_box_runner_for_tests.store(runner, std::memory_order_release);
}
#endif

}  // namespace internal

// static
ProgressDialog* ProgressDialog::GetCurrent() {
  return g_current_dialog;
}

ProgressDialog::ProgressDialog(HWND parent) : parent_(parent) {
  g_current_dialog = this;
  error_dialog_language_ = GetThreadUILanguage();

  // Load every string before starting the dialog thread. Windows resource
  // selection uses the current thread's preferred UI languages, which a newly
  // created thread does not inherit when the caller selected them explicitly.
  for (int i = 0; i < kNumSteps; i++) {
    step_strings_[i] = internal::LoadStringResource(kStepResourceIds[i], L"");
    if (step_strings_[i].empty()) {
      step_strings_[i] =
          base::UTF8ToWide(StepDisplayString(static_cast<Step>(i)));
    }
  }
  title_ = internal::LoadStringResource(IDS_INSTALLER_TITLE, L"CEF Installer");
  cancel_ = internal::LoadStringResource(IDS_INSTALLER_CANCEL, L"Cancel");
  error_title_ = internal::LoadStringResource(IDS_INSTALLER_ERROR_TITLE,
                                              L"Installation Error");
  error_message_format_ = internal::LoadStringResource(
      IDS_INSTALLER_ERROR_MESSAGE_FORMAT, kDefaultErrorMessageFormat);
  if (!HasUnescapedPlaceholder(error_message_format_, L'1') ||
      !HasUnescapedPlaceholder(error_message_format_, L'2')) {
    LOG(WARNING) << "Installer error message format must contain $1 and $2; "
                    "using the default format";
    error_message_format_ = kDefaultErrorMessageFormat;
  }
  error_messages_.reserve(std::size(kErrorMessageResources));
  for (const auto& resource : kErrorMessageResources) {
    error_messages_.push_back(
        internal::LoadStringResource(resource.id, resource.default_value));
  }
  percent_format_ =
      internal::LoadStringResource(IDS_INSTALLER_PERCENT_FORMAT, L"$1%");
}

void ProgressDialog::EnsureThreadStarted() {
  if (dialog_thread_.IsRunning()) {
    return;
  }

  scoped_thread_pool_.emplace();

  base::Thread::Options options(base::MessagePumpType::UI, 0);
  if (!dialog_thread_.StartWithOptions(std::move(options))) {
    LOG(ERROR) << "Failed to start dialog thread";
    return;
  }

  // Create the dialog window on the dialog thread and wait for completion.
  base::WaitableEvent created;
  dialog_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProgressDialog* self, HWND parent, base::WaitableEvent* event) {
            self->CreateDialogOnThread(parent);
            event->Signal();
          },
          base::Unretained(this), parent_, &created));
  created.Wait();
}

ProgressDialog::~ProgressDialog() {
  Close();
}

void ProgressDialog::CreateDialogOnThread(HWND parent) {
  InitCommonControls();

  // Validate the parent HWND. We only use it for positioning (read-only).
  // We do NOT pass it to CreateDialogParamW because:
  // 1. That would establish an owner relationship with an external window
  // 2. The parent may be from a different process with different trust level
  // 3. If owner is destroyed, Windows would destroy our dialog too
  if (parent && !IsWindow(parent)) {
    LOG(WARNING) << "Invalid parent HWND provided, ignoring";
    parent = nullptr;
  }

  // Create the dialog without an owner - parent_ is only used for positioning
  hwnd_ = CreateDialogParamW(GetModuleHandle(nullptr),
                             MAKEINTRESOURCEW(IDD_INSTALLER_PROGRESS), nullptr,
                             DialogProc, reinterpret_cast<LPARAM>(this));

  if (!hwnd_) {
    LOG(ERROR) << "Failed to create progress dialog: " << GetLastError();
    return;
  }

  // Cache control handles
  step_label_ = GetDlgItem(hwnd_, IDC_INSTALLER_STEP);
  percent_label_ = GetDlgItem(hwnd_, IDC_INSTALLER_PERCENT);
  progress_bar_ = GetDlgItem(hwnd_, IDC_INSTALLER_PROGRESS);
  cancel_button_ = GetDlgItem(hwnd_, IDC_INSTALLER_CANCEL);

  // Apply all textual dialog chrome from the executable's string table before
  // the window is shown. The .rc dialog text is only a resource-editor preview.
  SetWindowTextW(hwnd_, title_.c_str());
  if (cancel_button_) {
    SetWindowTextW(cancel_button_, cancel_.c_str());
  }
  DoSetStepText(step_strings_[kStepInit]);

  // Initialize progress bar range
  if (progress_bar_) {
    SendMessage(progress_bar_, PBM_SETRANGE32, 0, 100);
    SendMessage(progress_bar_, PBM_SETPOS, 0, 0);
  }
  DoSetProgress(0);

  // Position the dialog: center on parent window if valid, otherwise center
  // on primary monitor. We read the parent's position but never modify it.
  RECT rc;
  GetWindowRect(hwnd_, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;

  int x, y;
  RECT parent_rc;
  if (parent && !IsIconic(parent) && GetWindowRect(parent, &parent_rc)) {
    x = parent_rc.left + (parent_rc.right - parent_rc.left - width) / 2;
    y = parent_rc.top + (parent_rc.bottom - parent_rc.top - height) / 2;
  } else {
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
  }

  // Ensure the dialog lands on a valid monitor. If the computed position is
  // off-screen (e.g., parent on a disconnected display), clamp to the nearest
  // monitor's work area.
  RECT candidate = {x, y, x + width, y + height};
  HMONITOR monitor = MonitorFromRect(&candidate, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  if (GetMonitorInfo(monitor, &mi)) {
    const RECT& work = mi.rcWork;
    if (x + width > work.right) {
      x = work.right - width;
    }
    if (y + height > work.bottom) {
      y = work.bottom - height;
    }
    if (x < work.left) {
      x = work.left;
    }
    if (y < work.top) {
      y = work.top;
    }
  } else {
    // Shouldn't happen, but fall back to primary monitor center.
    x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
  }

  // Always topmost for visibility. We intentionally do NOT disable the parent
  // window or establish an owner relationship — the parent HWND may be from a
  // different process with a different trust level.
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

// static
INT_PTR CALLBACK ProgressDialog::DialogProc(HWND hwnd,
                                            UINT msg,
                                            WPARAM wparam,
                                            LPARAM lparam) {
  ProgressDialog* dialog = nullptr;

  if (msg == WM_INITDIALOG) {
    dialog = reinterpret_cast<ProgressDialog*>(lparam);
    SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(dialog));
    dialog->hwnd_ = hwnd;
    // Positioning is done in CreateDialogOnThread after this returns.
    return TRUE;
  }

  dialog =
      reinterpret_cast<ProgressDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));

  if (!dialog) {
    return FALSE;
  }

  switch (msg) {
    case WM_COMMAND:
      if (LOWORD(wparam) == IDC_INSTALLER_CANCEL ||
          LOWORD(wparam) == IDCANCEL) {
        if (dialog->cancellation_deferred_.load(std::memory_order_acquire)) {
          dialog->cancelled_.store(true, std::memory_order_release);
          dialog->DoSetCancelEnabled(false);
          return TRUE;
        }
        if (!dialog->cancelled_.exchange(true)) {
          dialog->DoSetCancelEnabled(false);
        }
        return TRUE;
      }
      break;

    case WM_CLOSE:
      if (dialog->cancellation_deferred_.load(std::memory_order_acquire)) {
        dialog->cancelled_.store(true, std::memory_order_release);
        dialog->DoSetCancelEnabled(false);
        return TRUE;
      }
      // Treat close button as cancel
      dialog->cancelled_.store(true, std::memory_order_release);
      ShowWindow(hwnd, SW_HIDE);
      return TRUE;

    case WM_DESTROY:
      dialog->hwnd_ = nullptr;
      return TRUE;
  }

  return FALSE;
}

void ProgressDialog::Show() {
  EnsureThreadStarted();
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](ProgressDialog* self) {
                         if (self->hwnd_) {
                           ShowWindow(self->hwnd_, SW_SHOW);
                           UpdateWindow(self->hwnd_);
                         }
                       },
                       base::Unretained(this)));
  }
}

void ProgressDialog::Hide() {
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](ProgressDialog* self) {
                         if (self->hwnd_) {
                           ShowWindow(self->hwnd_, SW_HIDE);
                         }
                       },
                       base::Unretained(this)));
  }
}

void ProgressDialog::SetStep(Step step) {
  if (step >= 0 && step < kNumSteps && dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProgressDialog::DoSetStepText,
                                  base::Unretained(this), step_strings_[step]));
  }
}

void ProgressDialog::SetProgress(int percent) {
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProgressDialog::DoSetProgress,
                                  base::Unretained(this), percent));
  }
}

void ProgressDialog::SetCancelEnabled(bool enabled) {
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ProgressDialog::DoSetCancelEnabled,
                                  base::Unretained(this), enabled));
  }
}

void ProgressDialog::SetCancellationDeferred(bool deferred) {
  if (cancellation_deferred_.load(std::memory_order_acquire) == deferred) {
    return;
  }
  if (!dialog_thread_.IsRunning()) {
    cancellation_deferred_.store(deferred, std::memory_order_release);
    return;
  }
  if (dialog_thread_.task_runner()->RunsTasksInCurrentSequence()) {
    DoSetCancellationDeferred(deferred);
    return;
  }
  base::WaitableEvent complete;
  dialog_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProgressDialog* self, bool value,
             base::WaitableEvent* completion) {
            self->DoSetCancellationDeferred(value);
            completion->Signal();
          },
          base::Unretained(this), deferred, base::Unretained(&complete)));
  complete.Wait();
}

bool ProgressDialog::WasCancelled() const {
  return cancelled_.load(std::memory_order_acquire);
}

void ProgressDialog::ShowErrorDialog(int error_code) {
  const ErrorMessageKind message_kind = ErrorCodeToMessageKind(error_code);
  const std::u16string body =
      base::WideToUTF16(error_messages_[static_cast<size_t>(message_kind)]);
  const std::u16string code = base::NumberToString16(error_code);
  const std::array<std::u16string, 2> substitutions = {body, code};
  std::wstring message = base::UTF16ToWide(base::ReplaceStringPlaceholders(
      base::WideToUTF16(error_message_format_),
      base::span<const std::u16string>(substitutions), nullptr));

  if (!dialog_thread_.IsRunning()) {
    // Dialog thread not running; show standalone.
    ShowErrorMessageBox(nullptr, error_title_, message, error_dialog_language_);
    return;
  }
  base::WaitableEvent dismissed;
  dialog_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](ProgressDialog* self, const std::wstring& t, const std::wstring& m,
             LANGID language_id, base::WaitableEvent* event) {
            ShowErrorMessageBox(self->hwnd_, t, m, language_id);
            event->Signal();
          },
          base::Unretained(this), error_title_, message, error_dialog_language_,
          &dismissed));
  dismissed.Wait();
}

void ProgressDialog::Close() {
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProgressDialog::DoClose, base::Unretained(this)));
    dialog_thread_.Stop();
  }
  if (g_current_dialog == this) {
    g_current_dialog = nullptr;
  }
}

HWND ProgressDialog::GetHwnd() const {
  return hwnd_;
}

bool ProgressDialog::IsVisible() const {
  return hwnd_ && IsWindowVisible(hwnd_);
}

void ProgressDialog::FlushForTesting() {
  if (dialog_thread_.IsRunning()) {
    dialog_thread_.FlushForTesting();
  }
}

// --- Methods that run on the dialog thread ---

void ProgressDialog::DoSetStepText(const std::wstring& step_text) {
  if (step_label_) {
    SetWindowTextW(step_label_, step_text.c_str());
  }
}

void ProgressDialog::DoSetProgress(int percent) {
  if (percent < 0) {
    percent = 0;
  }
  if (percent > 100) {
    percent = 100;
  }

  if (progress_bar_) {
    SendMessage(progress_bar_, PBM_SETPOS, percent, 0);
  }

  if (percent_label_) {
    const std::u16string percent_text = base::NumberToString16(percent);
    const std::wstring label =
        base::UTF16ToWide(base::ReplaceStringPlaceholders(
            base::WideToUTF16(percent_format_),
            base::span<const std::u16string>(&percent_text, 1u), nullptr));
    SetWindowTextW(percent_label_, label.c_str());
  }
}

void ProgressDialog::DoSetCancelEnabled(bool enabled) {
  if (cancel_button_) {
    EnableWindow(cancel_button_, enabled ? TRUE : FALSE);
  }
}

void ProgressDialog::DoSetCancellationDeferred(bool deferred) {
  cancellation_deferred_.store(deferred, std::memory_order_release);
  if (deferred && !cancelled_.load(std::memory_order_acquire)) {
    DoSetCancelEnabled(true);
  }
}

void ProgressDialog::DoClose() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}

}  // namespace cef_installer
