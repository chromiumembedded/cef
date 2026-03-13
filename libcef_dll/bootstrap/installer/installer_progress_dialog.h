// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PROGRESS_DIALOG_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PROGRESS_DIALOG_H_

#include <windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/threading/thread.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_scoped_thread_pool.h"

namespace cef_installer {

// Progress dialog for installer operations.
// Uses a Windows dialog resource with progress bar and static text.
//
// The dialog runs on a dedicated UI thread with its own message loop, so it
// remains responsive even when the calling thread is blocked on I/O or
// network operations. All public methods are thread-safe and communicate
// with the dialog thread via PostTask.
//
// Usage:
//   ProgressDialog dialog(parent_hwnd);
//   dialog.Show();
//   dialog.SetStep(kStepDownload);
//   dialog.SetProgress(50);
//   // ... later ...
//   dialog.Close();
class ProgressDialog {
 public:
  // Create a progress dialog.
  // If parent is non-null, the dialog is positioned relative to that window.
  // If parent is null, the dialog is standalone and topmost.
  // The dialog thread is not started until Show() is called.
  explicit ProgressDialog(HWND parent = nullptr);
  ~ProgressDialog();

  ProgressDialog(const ProgressDialog&) = delete;
  ProgressDialog& operator=(const ProgressDialog&) = delete;

  // Show the dialog.
  void Show();

  // Hide the dialog without destroying it.
  void Hide();

  // Set the current step using the localized string cached at construction.
  void SetStep(Step step);

  // Set progress as a percentage (0-100 for determinate progress).
  void SetProgress(int percent);

  // Enable or disable the Cancel button.
  void SetCancelEnabled(bool enabled);

  // Defer cancellation requests while leaving the Cancel button enabled.
  // Requests racing with entry remain pending for the next safe cancellation
  // checkpoint.
  void SetCancellationDeferred(bool deferred);

  // Check if user has requested cancellation. Thread-safe (atomic).
  bool WasCancelled() const;

  // Show a modal error dialog parented to this progress dialog.
  // Uses the localized title, format, and error-code-specific guidance cached
  // at construction. Runs on the dialog thread so the parent HWND stays
  // same-thread. Blocks until the user dismisses the dialog.
  void ShowErrorDialog(int error_code);

  // Close and destroy the dialog window. Blocks until the dialog thread
  // has stopped.
  void Close();

  // Get the native window handle. Test-only.
  // WARNING: hwnd_ is owned by the dialog thread. This is only safe to call
  // after Show() (which synchronizes) and before Close(). Production
  // code should not use this — use PostTask-based methods instead.
  HWND GetHwnd() const;

  // Check if the dialog window is currently visible. Test-only.
  // Same threading caveats as GetHwnd().
  bool IsVisible() const;

  // Flush pending tasks on the dialog thread. Test-only.
  void FlushForTesting();

  // Returns the currently active ProgressDialog instance, or nullptr.
  // Only one ProgressDialog exists at a time within a process.
  static ProgressDialog* GetCurrent();

 private:
  // Dialog procedure for the dialog resource.
  static INT_PTR CALLBACK DialogProc(HWND hwnd,
                                     UINT msg,
                                     WPARAM wparam,
                                     LPARAM lparam);

  // Methods that run on the dialog thread (called via PostTask).
  void CreateDialogOnThread(HWND parent);
  void DoSetStepText(const std::wstring& step_text);
  void DoSetProgress(int percent);
  void DoSetCancelEnabled(bool enabled);
  void DoSetCancellationDeferred(bool deferred);
  void DoClose();

  void EnsureThreadStarted();

  HWND parent_ = nullptr;
  std::optional<ScopedThreadPool> scoped_thread_pool_;
  base::Thread dialog_thread_{"InstallerProgressDialog"};
  std::atomic<bool> cancelled_{false};
  std::atomic<bool> cancellation_deferred_{false};

  // The following members should only be accessed on the dialog thread.
  HWND hwnd_ = nullptr;
  HWND step_label_ = nullptr;
  HWND percent_label_ = nullptr;
  HWND progress_bar_ = nullptr;
  HWND cancel_button_ = nullptr;

  // Immutable localized strings. Load all resources on the constructing
  // thread so an explicitly selected thread UI language is applied
  // consistently before the dialog thread starts.
  std::array<std::wstring, kNumSteps> step_strings_;
  std::wstring title_;
  std::wstring cancel_;
  std::wstring error_title_;
  std::wstring error_message_format_;
  std::vector<std::wstring> error_messages_;
  std::wstring percent_format_;
  LANGID error_dialog_language_ = 0;
};

// ============================================================================
// Internal helpers exposed for testing. Not part of the public API.
// ============================================================================
namespace internal {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
using StringResourceLoader = std::wstring (*)(UINT id,
                                              const wchar_t* default_value);
using MessageBoxRunner = int (*)(HWND parent,
                                 const wchar_t* message,
                                 const wchar_t* title,
                                 UINT flags,
                                 WORD language_id);

// Load a string resource with fallback to default value.
// Uses the current module's resources.
std::wstring LoadStringResource(UINT id, const wchar_t* default_value);

// Overrides string loading for tests. |loader| must remain callable until the
// override is cleared.
void SetStringResourceLoaderForTesting(StringResourceLoader loader);

// Overrides error message-box display for tests. |runner| must remain callable
// until the override is cleared.
void SetMessageBoxRunnerForTesting(MessageBoxRunner runner);
#endif

}  // namespace internal

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_PROGRESS_DIALOG_H_
