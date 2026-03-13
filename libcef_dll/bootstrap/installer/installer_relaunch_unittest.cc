// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_relaunch.h"

#include <windows.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/installer/installer_bootstrap_helpers.h"
#include "cef/libcef_dll/bootstrap/installer/installer_lifecycle_test_support.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cef_installer {
namespace {

using test::InstallerLifecycleCorrelator;
using test::InstallerLifecycleParseStatus;
using test::ParsedInstallerLifecycleEvent;
using test::ParseInstallerLifecyclePayload;

struct RelaunchCapture {
  std::mutex mutex;
  bool launch_success = true;
  DWORD next_pid = 7000;
  InstallerLifecycleSendStatus send_status =
      InstallerLifecycleSendStatus::kDelivered;
  int launch_calls = 0;
  int send_calls = 0;
  int copy_client_calls = 0;
  bool copy_client_success = true;
  std::vector<base::FilePath> executables;
  std::vector<base::FilePath> directories;
  std::vector<std::wstring> command_lines;
  std::vector<std::string> lifecycle_payloads;
  std::vector<base::FilePath> client_copy_sources;
  std::vector<base::FilePath> client_copy_destinations;
};

RelaunchCapture* g_relaunch_capture = nullptr;

bool CaptureLaunch(const base::FilePath& executable,
                   std::wstring_view command_line,
                   const base::FilePath& current_directory,
                   DWORD* child_pid) {
  CHECK(g_relaunch_capture);
  std::lock_guard lock(g_relaunch_capture->mutex);
  ++g_relaunch_capture->launch_calls;
  g_relaunch_capture->executables.push_back(executable);
  g_relaunch_capture->directories.push_back(current_directory);
  g_relaunch_capture->command_lines.emplace_back(command_line);
  if (!g_relaunch_capture->launch_success) {
    ::SetLastError(ERROR_ACCESS_DENIED);
    return false;
  }
  *child_pid = g_relaunch_capture->next_pid++;
  return true;
}

InstallerLifecycleSendStatus CaptureLifecycleSend(HWND, std::string_view json) {
  CHECK(g_relaunch_capture);
  std::lock_guard lock(g_relaunch_capture->mutex);
  ++g_relaunch_capture->send_calls;
  g_relaunch_capture->lifecycle_payloads.emplace_back(json);
  return g_relaunch_capture->send_status;
}

bool CaptureClientModuleCopy(const base::FilePath& source,
                             const base::FilePath& destination) {
  CHECK(g_relaunch_capture);
  std::lock_guard lock(g_relaunch_capture->mutex);
  ++g_relaunch_capture->copy_client_calls;
  g_relaunch_capture->client_copy_sources.push_back(source);
  g_relaunch_capture->client_copy_destinations.push_back(destination);
  return g_relaunch_capture->copy_client_success &&
         base::CopyFile(source, destination);
}

class RelaunchLifecycleWindow {
 public:
  RelaunchLifecycleWindow() {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = ::GetModuleHandle(nullptr);
    window_class.lpszClassName = L"CefInstallerRelaunchLifecycleUnitTest";
    ::RegisterClassW(&window_class);
    window_ =
        ::CreateWindowW(window_class.lpszClassName, L"", 0, 0, 0, 0, 0,
                        HWND_MESSAGE, nullptr, window_class.hInstance, this);
  }

  ~RelaunchLifecycleWindow() {
    if (window_) {
      ::DestroyWindow(window_);
    }
  }

  HWND window() const { return window_; }
  const std::vector<std::string>& payloads() const { return payloads_; }

 private:
  static LRESULT CALLBACK WindowProc(HWND window,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
    RelaunchLifecycleWindow* self = reinterpret_cast<RelaunchLifecycleWindow*>(
        ::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
      self = static_cast<RelaunchLifecycleWindow*>(create->lpCreateParams);
      ::SetWindowLongPtrW(window, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self));
    }
    if (message == WM_COPYDATA && self) {
      auto* data = reinterpret_cast<COPYDATASTRUCT*>(lparam);
      if (data && data->dwData == kWmCopyDataInstallerLifecycle &&
          data->lpData) {
        self->payloads_.emplace_back(static_cast<const char*>(data->lpData),
                                     data->cbData);
        return 1;
      }
    }
    return ::DefWindowProcW(window, message, wparam, lparam);
  }

  HWND window_ = nullptr;
  std::vector<std::string> payloads_;
};

class InstallerRelaunchTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    internal::SetUninstallRelaunchCallbacksForTesting(nullptr);
    g_relaunch_capture = nullptr;
    for (const auto& directory : capture_.directories) {
      base::DeletePathRecursively(directory);
    }
    for (HMODULE module : loaded_modules_) {
      ::FreeLibrary(module);
    }
  }

  void InstallCallbacks(bool intercept_send) {
    g_relaunch_capture = &capture_;
    callbacks_.launch = CaptureLaunch;
    callbacks_.send_lifecycle = intercept_send ? CaptureLifecycleSend : nullptr;
    callbacks_.copy_client_module = CaptureClientModuleCopy;
    internal::SetUninstallRelaunchCallbacksForTesting(&callbacks_);
  }

  base::FilePath CreateRelaunchSource() {
    const base::FilePath source = temp_dir_.GetPath().Append(L"source");
    EXPECT_TRUE(base::CreateDirectory(source));
    EXPECT_TRUE(base::WriteFile(source.Append(L"host.exe"), "mock exe"));
    EXPECT_TRUE(
        base::WriteFile(source.Append(L"chrome_elf.dll"), "mock chrome elf"));
    EXPECT_TRUE(base::WriteFile(source.Append(L"crash_reporter.cfg"),
                                "mock crash config"));
    return source.Append(L"host.exe");
  }

  HMODULE LoadTestModule(const wchar_t* basename) {
    base::FilePath executable_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_dir));
    HMODULE module =
        ::LoadLibraryExW(executable_dir.Append(basename).value().c_str(),
                         nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (module) {
      loaded_modules_.push_back(module);
    }
    return module;
  }

  std::vector<std::wstring> EnumerateFileNames(const base::FilePath& dir) {
    std::vector<std::wstring> names;
    base::FileEnumerator files(dir, false, base::FileEnumerator::FILES);
    for (base::FilePath path = files.Next(); !path.empty();
         path = files.Next()) {
      names.push_back(path.BaseName().value());
    }
    std::ranges::sort(names);
    return names;
  }

  std::string CapturedNonce(size_t index) {
    constexpr std::wstring_view marker = L"--cef-uninstall-state=";
    const std::wstring& command_line = capture_.command_lines.at(index);
    const size_t position = command_line.find(marker);
    EXPECT_NE(std::wstring::npos, position);
    if (position == std::wstring::npos) {
      return {};
    }
    return base::WideToUTF8(command_line.substr(position + marker.size()));
  }

  base::ScopedTempDir temp_dir_;
  RelaunchCapture capture_;
  internal::UninstallRelaunchCallbacksForTesting callbacks_;
  std::vector<HMODULE> loaded_modules_;
};

// ============================================================================
// GetUninstallTempDirectory Tests
// ============================================================================

TEST_F(InstallerRelaunchTest, GetUninstallTempDirectory) {
  base::FilePath temp_path = GetUninstallTempDirectory();

  // Should return a valid path
  EXPECT_FALSE(temp_path.empty());

  // Should be under %TEMP%
  wchar_t system_temp[MAX_PATH];
  DWORD len = GetTempPathW(MAX_PATH, system_temp);
  ASSERT_GT(len, 0u);

  EXPECT_EQ(base::FilePath(system_temp).StripTrailingSeparators(),
            temp_path.DirName());

  // Should contain the expected prefix
  EXPECT_EQ(0u, temp_path.BaseName().value().find(L"cef_uninstall_"));
}

TEST_F(InstallerRelaunchTest, GetUninstallTempDirectoryUnique) {
  // Multiple calls should return different paths
  base::FilePath path1 = GetUninstallTempDirectory();
  base::FilePath path2 = GetUninstallTempDirectory();

  EXPECT_FALSE(path1.empty());
  EXPECT_FALSE(path2.empty());
  EXPECT_NE(path1, path2);
}

TEST_F(InstallerRelaunchTest, RelaunchStatePreservesAbsoluteInstallPath) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  const std::string install_path = temp_dir_.GetPath().AsUTF8Unsafe();

  constexpr std::string_view kNonce = "test-nonce";
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(relaunch_dir, install_path,
                                                    kNonce));
  std::string loaded_path;
  EXPECT_TRUE(
      internal::ReadUninstallRelaunchState(exe_path, kNonce, &loaded_path));
  EXPECT_EQ(install_path, loaded_path);

  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateAcceptsEmptyDefaultSearch) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));

  constexpr std::string_view kNonce = "test-nonce";
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(relaunch_dir, "", kNonce));
  std::string loaded_path = "stale";
  EXPECT_TRUE(
      internal::ReadUninstallRelaunchState(exe_path, kNonce, &loaded_path));
  EXPECT_TRUE(loaded_path.empty());

  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateWriterMatchesReaderSizeLimit) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  constexpr std::string_view kNonce = "test-nonce";
  HWND parent = reinterpret_cast<HWND>(static_cast<uintptr_t>(12345));
  UninstallLifecycleContext context{"0123456789abcdef0123456789abcdef", parent};

  std::string install_path = "C:/a";
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(relaunch_dir, install_path,
                                                    kNonce, context));
  const base::FilePath state_path =
      relaunch_dir.Append(L"cef_uninstall_state.json");
  std::string serialized;
  ASSERT_TRUE(base::ReadFileToString(state_path, &serialized));
  ASSERT_LT(serialized.size(), internal::kMaxUninstallRelaunchStateSize);
  install_path.append(
      internal::kMaxUninstallRelaunchStateSize - serialized.size(), 'a');

  ASSERT_TRUE(internal::WriteUninstallRelaunchState(relaunch_dir, install_path,
                                                    kNonce, context));
  ASSERT_TRUE(base::ReadFileToString(state_path, &serialized));
  EXPECT_EQ(internal::kMaxUninstallRelaunchStateSize, serialized.size());
  UninstallRelaunchState loaded;
  ASSERT_TRUE(
      internal::ReadUninstallRelaunchState(exe_path, kNonce, parent, &loaded));
  EXPECT_EQ(install_path, loaded.install_path);
  ASSERT_TRUE(loaded.lifecycle_context);

  install_path.push_back('a');
  EXPECT_FALSE(internal::WriteUninstallRelaunchState(relaunch_dir, install_path,
                                                     kNonce, context));
  ASSERT_TRUE(base::ReadFileToString(state_path, &serialized));
  EXPECT_EQ(internal::kMaxUninstallRelaunchStateSize, serialized.size());
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateLifecycleContextRoundTrip) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  constexpr std::string_view kNonce = "test-nonce";
  constexpr char kOperationId[] = "0123456789abcdef0123456789abcdef";
  HWND parent = reinterpret_cast<HWND>(static_cast<uintptr_t>(12345));
  UninstallLifecycleContext context{kOperationId, parent};
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), kNonce, context));

  UninstallRelaunchState loaded;
  ASSERT_TRUE(
      internal::ReadUninstallRelaunchState(exe_path, kNonce, parent, &loaded));
  EXPECT_EQ(temp_dir_.GetPath().AsUTF8Unsafe(), loaded.install_path);
  ASSERT_TRUE(loaded.lifecycle_context);
  EXPECT_EQ(kOperationId, loaded.lifecycle_context->operation_id);
  EXPECT_EQ(parent, loaded.lifecycle_context->parent_window);
  EXPECT_FALSE(internal::ReadUninstallRelaunchState(
      exe_path, kNonce, reinterpret_cast<HWND>(static_cast<uintptr_t>(12346)),
      &loaded));
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest,
       ResolveInvocationContextLoadsTrustedRelaunchState) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  constexpr std::string_view kNonce = "test-nonce";
  constexpr char kOperationId[] = "0123456789abcdef0123456789abcdef";
  HWND parent = reinterpret_cast<HWND>(static_cast<uintptr_t>(12345));
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), kNonce,
      UninstallLifecycleContext{kOperationId, parent}));

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kSwitchUninstall);
  command_line.AppendSwitch(kSwitchUninstallRelaunched);
  command_line.AppendSwitchASCII(kSwitchUninstallState, kNonce);
  command_line.AppendSwitchASCII(
      kSwitchParentWindow,
      base::NumberToString(reinterpret_cast<uintptr_t>(parent)));

  const auto context = ResolveUninstallInvocationContext(command_line, exe_path,
                                                         Command::kUninstall);
  ASSERT_TRUE(context);
  EXPECT_EQ(UninstallInvocationState::kRelaunched, context->invocation);
  EXPECT_EQ(temp_dir_.GetPath().AsUTF8Unsafe(),
            context->relaunch_state.install_path);
  ASSERT_TRUE(context->relaunch_state.lifecycle_context);
  EXPECT_EQ(kOperationId,
            context->relaunch_state.lifecycle_context->operation_id);
  EXPECT_EQ(parent, context->relaunch_state.lifecycle_context->parent_window);

  base::CommandLine missing_parent(base::CommandLine::NO_PROGRAM);
  missing_parent.AppendSwitch(kSwitchUninstall);
  missing_parent.AppendSwitch(kSwitchUninstallRelaunched);
  missing_parent.AppendSwitchASCII(kSwitchUninstallState, kNonce);
  EXPECT_FALSE(ResolveUninstallInvocationContext(missing_parent, exe_path,
                                                 Command::kUninstall));
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest,
       ResolveInvocationContextRejectsMalformedOrUntrustedTransport) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  constexpr std::string_view kNonce = "test-nonce";
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), kNonce));

  struct TestCase {
    std::optional<Command> command;
    bool marker;
    std::optional<std::string_view> nonce;
  };
  const TestCase cases[] = {
      {Command::kUninstall, true, std::nullopt},
      {Command::kUninstall, false, kNonce},
      {Command::kUpdate, true, kNonce},
      {std::nullopt, true, kNonce},
      {Command::kUninstall, true, "wrong-nonce"},
  };
  for (const auto& test : cases) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (test.command) {
      command_line.AppendSwitch(test.command == Command::kUninstall
                                    ? kSwitchUninstall
                                    : kSwitchUpdate);
    }
    if (test.marker) {
      command_line.AppendSwitch(kSwitchUninstallRelaunched);
    }
    if (test.nonce) {
      command_line.AppendSwitchASCII(kSwitchUninstallState, *test.nonce);
    }
    EXPECT_FALSE(ResolveUninstallInvocationContext(command_line, exe_path,
                                                   test.command));
  }
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest,
       ResolveInvocationContextClassifiesNonRelaunchCommands) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  auto original = ResolveUninstallInvocationContext(
      command_line, base::FilePath(), Command::kUninstall);
  ASSERT_TRUE(original);
  EXPECT_EQ(UninstallInvocationState::kOriginal, original->invocation);

  auto update = ResolveUninstallInvocationContext(
      command_line, base::FilePath(), Command::kUpdate);
  ASSERT_TRUE(update);
  EXPECT_EQ(UninstallInvocationState::kNotUninstall, update->invocation);
}

TEST_F(InstallerRelaunchTest,
       RelaunchStateRejectsPartialAndMalformedLifecycle) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  const base::FilePath state_path =
      relaunch_dir.Append(L"cef_uninstall_state.json");
  constexpr char kOperationId[] = "0123456789abcdef0123456789abcdef";
  HWND parent = reinterpret_cast<HWND>(static_cast<uintptr_t>(12345));

  const std::vector<
      std::pair<std::optional<std::string>, std::optional<std::string>>>
      cases = {
          {kOperationId, std::nullopt},
          {std::nullopt, "12345"},
          {"0123456789ABCDEF0123456789ABCDEF", "12345"},
          {kOperationId, "012345"},
          {kOperationId, "0"},
          {kOperationId, "18446744073709551616"},
      };
  for (const auto& [operation_id, parent_window] : cases) {
    base::DictValue state;
    state.Set("install_path", temp_dir_.GetPath().AsUTF8Unsafe());
    state.Set("nonce", "test-nonce");
    if (operation_id) {
      state.Set("operation_id", *operation_id);
    }
    if (parent_window) {
      state.Set("parent_window", *parent_window);
    }
    std::string json;
    ASSERT_TRUE(base::JSONWriter::Write(state, &json));
    ASSERT_TRUE(base::WriteFile(state_path, json));
    UninstallRelaunchState loaded;
    EXPECT_FALSE(internal::ReadUninstallRelaunchState(exe_path, "test-nonce",
                                                      parent, &loaded))
        << json;
  }
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest,
       RelaunchStateAcceptsParentDestroyedAfterPublication) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  HWND parent =
      ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                        ::GetModuleHandle(nullptr), nullptr);
  ASSERT_TRUE(parent);
  UninstallLifecycleContext context{"0123456789abcdef0123456789abcdef", parent};
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), "test-nonce", context));
  ASSERT_TRUE(::DestroyWindow(parent));
  EXPECT_FALSE(::IsWindow(parent));

  UninstallRelaunchState loaded;
  ASSERT_TRUE(internal::ReadUninstallRelaunchState(exe_path, "test-nonce",
                                                   parent, &loaded));
  ASSERT_TRUE(loaded.lifecycle_context);
  EXPECT_EQ(parent, loaded.lifecycle_context->parent_window);
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateRejectsRelativeInstallPath) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  EXPECT_FALSE(internal::WriteUninstallRelaunchState(
      relaunch_dir, "relative_store", "test-nonce"));
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateRejectsNonceMismatch) {
  const base::FilePath relaunch_dir = GetUninstallTempDirectory();
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), "expected-nonce"));

  std::string loaded_path;
  EXPECT_FALSE(internal::ReadUninstallRelaunchState(exe_path, "wrong-nonce",
                                                    &loaded_path));
  EXPECT_TRUE(base::DeletePathRecursively(relaunch_dir));
}

TEST_F(InstallerRelaunchTest, RelaunchStateRejectsTempPrefixSibling) {
  const base::FilePath temp_root = GetTempDirectoryPath();
  ASSERT_FALSE(temp_root.empty());
  const std::wstring sibling_prefix = temp_root.BaseName().value() + L"Evil_";
  base::ScopedTempDir prefix_sibling;
  ASSERT_TRUE(prefix_sibling.CreateUniqueTempDirUnderPath(temp_root.DirName(),
                                                          sibling_prefix));

  const base::FilePath relaunch_dir =
      prefix_sibling.GetPath().Append(L"cef_uninstall_prefix_sibling");
  ASSERT_TRUE(base::CreateDirectory(relaunch_dir));
  const base::FilePath exe_path = relaunch_dir.Append(L"host.exe");
  ASSERT_TRUE(base::WriteFile(exe_path, "mock executable"));
  constexpr std::string_view kNonce = "test-nonce";
  ASSERT_TRUE(internal::WriteUninstallRelaunchState(
      relaunch_dir, temp_dir_.GetPath().AsUTF8Unsafe(), kNonce));

  std::string loaded_path;
  EXPECT_FALSE(
      internal::ReadUninstallRelaunchState(exe_path, kNonce, &loaded_path));
}

// ============================================================================
// CopySelfToTempAndRelaunch Tests
// ============================================================================

TEST_F(InstallerRelaunchTest,
       ClientResourceCopyUsesModuleBasenameAndPreservesArguments) {
  InstallCallbacks(/*intercept_send=*/true);
  const base::FilePath exe_path = CreateRelaunchSource();
  HMODULE client_module = LoadTestModule(L"cef_config_test_appid_a.dll");
  ASSERT_TRUE(client_module);
  HWND parent =
      ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                        ::GetModuleHandle(nullptr), nullptr);
  ASSERT_TRUE(parent);

  const UninstallRelaunchResult result = CopySelfToTempAndRelaunch(
      exe_path, client_module,
      L"/cef-uninstall /cef-headless --module=cef_config_test_appid_a",
      temp_dir_.GetPath().AsUTF8Unsafe(), parent);
  ASSERT_TRUE(result.started());
  EXPECT_EQ(7000u, result.child_pid);
  EXPECT_TRUE(IsValidInstallerLifecycleOperationId(result.operation_id));
  ASSERT_EQ(1, capture_.launch_calls);
  ASSERT_EQ(1, capture_.send_calls);
  ASSERT_EQ(1u, capture_.directories.size());
  EXPECT_TRUE(base::PathExists(capture_.executables[0]));
  EXPECT_TRUE(
      base::PathExists(capture_.directories[0].Append(L"chrome_elf.dll")));
  ASSERT_EQ(1, capture_.copy_client_calls);
  EXPECT_EQ(L"cef_config_test_appid_a.dll",
            capture_.client_copy_sources[0].BaseName().value());
  EXPECT_EQ(L"cef_config_test_appid_a.dll",
            capture_.client_copy_destinations[0].BaseName().value());
  EXPECT_NE(std::wstring::npos, capture_.command_lines[0].find(
                                    L"/cef-uninstall /cef-headless "
                                    L"--module=cef_config_test_appid_a"));

  std::vector<std::wstring> expected_files = {
      L"cef_config_test_appid_a.dll", L"cef_uninstall_state.json",
      L"chrome_elf.dll", L"crash_reporter.cfg", L"host.exe"};
  std::ranges::sort(expected_files);
  EXPECT_EQ(expected_files, EnumerateFileNames(capture_.directories[0]));

  UninstallRelaunchState state;
  ASSERT_TRUE(internal::ReadUninstallRelaunchState(
      capture_.executables[0], CapturedNonce(0), parent, &state));
  ASSERT_TRUE(state.lifecycle_context);
  EXPECT_EQ(result.operation_id, state.lifecycle_context->operation_id);
  EXPECT_EQ(parent, state.lifecycle_context->parent_window);

  ASSERT_EQ(1u, capture_.lifecycle_payloads.size());
  std::string payload = capture_.lifecycle_payloads[0];
  payload.push_back('\0');
  const ParsedInstallerLifecycleEvent handoff =
      ParseInstallerLifecyclePayload(payload);
  ASSERT_EQ(InstallerLifecycleParseStatus::kValid, handoff.status);
  EXPECT_EQ(result.operation_id, handoff.operation_id);
  EXPECT_EQ(result.child_pid, handoff.child_pid);
  EXPECT_TRUE(::DestroyWindow(parent));
}

TEST_F(InstallerRelaunchTest, BootstrapResourceCopiesNoClientDll) {
  InstallCallbacks(/*intercept_send=*/true);
  const UninstallRelaunchResult result = CopySelfToTempAndRelaunch(
      CreateRelaunchSource(), nullptr, L"/cef-uninstall /cef-background");
  ASSERT_TRUE(result.started());
  EXPECT_EQ(0, capture_.copy_client_calls);
  ASSERT_EQ(1u, capture_.directories.size());
  std::vector<std::wstring> expected_files = {
      L"cef_uninstall_state.json", L"chrome_elf.dll", L"crash_reporter.cfg",
      L"host.exe"};
  std::ranges::sort(expected_files);
  EXPECT_EQ(expected_files, EnumerateFileNames(capture_.directories[0]));
  EXPECT_NE(std::wstring::npos,
            capture_.command_lines[0].find(L"/cef-uninstall /cef-background"));
}

TEST_F(InstallerRelaunchTest, ClientModuleCopyFailureCleansAndSendsNothing) {
  InstallCallbacks(/*intercept_send=*/true);
  capture_.copy_client_success = false;
  HMODULE client_module = LoadTestModule(L"cef_config_test_appid_a.dll");
  ASSERT_TRUE(client_module);
  const UninstallRelaunchResult result = CopySelfToTempAndRelaunch(
      CreateRelaunchSource(), client_module, L"/cef-uninstall");
  EXPECT_FALSE(result.started());
  EXPECT_EQ(1, capture_.copy_client_calls);
  EXPECT_EQ(0, capture_.launch_calls);
  EXPECT_EQ(0, capture_.send_calls);
  ASSERT_EQ(1u, capture_.client_copy_destinations.size());
  EXPECT_FALSE(
      base::PathExists(capture_.client_copy_destinations[0].DirName()));
}

TEST_F(InstallerRelaunchTest, CopySelfLaunchFailureCleansAndSendsNothing) {
  InstallCallbacks(/*intercept_send=*/true);
  capture_.launch_success = false;
  const UninstallRelaunchResult result = CopySelfToTempAndRelaunch(
      CreateRelaunchSource(), nullptr, L"/cef-uninstall");
  EXPECT_FALSE(result.started());
  ASSERT_EQ(1, capture_.launch_calls);
  EXPECT_EQ(0, capture_.send_calls);
  ASSERT_EQ(1u, capture_.directories.size());
  EXPECT_FALSE(base::PathExists(capture_.directories[0]));
}

TEST_F(InstallerRelaunchTest,
       CopySelfInvalidParentAndTimeoutDoNotChangeLaunchResult) {
  InstallCallbacks(/*intercept_send=*/true);
  const base::FilePath exe_path = CreateRelaunchSource();
  HWND destroyed =
      ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                        ::GetModuleHandle(nullptr), nullptr);
  ASSERT_TRUE(destroyed);
  ASSERT_TRUE(::DestroyWindow(destroyed));
  UninstallRelaunchResult result = CopySelfToTempAndRelaunch(
      exe_path, nullptr, L"/cef-uninstall", {}, destroyed);
  ASSERT_TRUE(result.started());
  EXPECT_TRUE(result.operation_id.empty());
  EXPECT_EQ(0, capture_.send_calls);

  HWND parent =
      ::CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                        ::GetModuleHandle(nullptr), nullptr);
  ASSERT_TRUE(parent);
  capture_.send_status = InstallerLifecycleSendStatus::kTimeout;
  result = CopySelfToTempAndRelaunch(exe_path, nullptr, L"/cef-uninstall", {},
                                     parent);
  ASSERT_TRUE(result.started());
  EXPECT_TRUE(IsValidInstallerLifecycleOperationId(result.operation_id));
  EXPECT_EQ(1, capture_.send_calls);
  EXPECT_TRUE(::DestroyWindow(parent));
}

TEST_F(InstallerRelaunchTest,
       ConcurrentHelpersShareReceiverWithoutCrossCorrelation) {
  InstallCallbacks(/*intercept_send=*/true);
  RelaunchLifecycleWindow receiver;
  ASSERT_TRUE(receiver.window());
  const base::FilePath exe_path = CreateRelaunchSource();
  UninstallRelaunchResult first;
  UninstallRelaunchResult second;
  std::thread first_thread([&]() {
    first = CopySelfToTempAndRelaunch(exe_path, nullptr, L"/cef-uninstall", {},
                                      receiver.window());
  });
  std::thread second_thread([&]() {
    second = CopySelfToTempAndRelaunch(exe_path, nullptr, L"/cef-uninstall", {},
                                       receiver.window());
  });
  first_thread.join();
  second_thread.join();
  ASSERT_TRUE(first.started());
  ASSERT_TRUE(second.started());
  ASSERT_NE(first.operation_id, second.operation_id);
  ASSERT_EQ(2u, capture_.lifecycle_payloads.size());
  for (const std::string& handoff : capture_.lifecycle_payloads) {
    ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
              SendInstallerLifecycleMessage(receiver.window(), handoff));
  }
  ASSERT_EQ(2u, receiver.payloads().size());

  const Result success = Result::Success({}, "");
  ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(
                receiver.window(), *SerializeInstallerOperationResult(
                                       second.operation_id, success, 0)));
  ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(receiver.window(),
                                          *SerializeInstallerOperationResult(
                                              first.operation_id, success, 0)));

  constexpr char kTerminalFirstId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  constexpr char kMissingHandoffId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(receiver.window(),
                                          *SerializeInstallerOperationResult(
                                              kTerminalFirstId, success, 0)));
  ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(
                receiver.window(),
                *SerializeInstallerRelaunchStarted(kTerminalFirstId, 9001)));
  ASSERT_EQ(InstallerLifecycleSendStatus::kDelivered,
            SendInstallerLifecycleMessage(receiver.window(),
                                          *SerializeInstallerOperationResult(
                                              kMissingHandoffId, success, 0)));

  InstallerLifecycleCorrelator correlator;
  for (const std::string& payload : receiver.payloads()) {
    const ParsedInstallerLifecycleEvent parsed =
        ParseInstallerLifecyclePayload(payload);
    ASSERT_EQ(InstallerLifecycleParseStatus::kValid, parsed.status);
    correlator.Accept(parsed);
  }
  EXPECT_TRUE(correlator.GetCorrelatedResult(first.operation_id));
  EXPECT_TRUE(correlator.GetCorrelatedResult(second.operation_id));
  EXPECT_TRUE(correlator.GetCorrelatedResult(kTerminalFirstId));
  EXPECT_FALSE(correlator.GetCorrelatedResult(kMissingHandoffId));
}

TEST_F(InstallerRelaunchTest, CopySelfEmptyPathFails) {
  base::FilePath empty_path;
  EXPECT_FALSE(CopySelfToTempAndRelaunch(empty_path, nullptr, L"").started());
}

}  // namespace
}  // namespace cef_installer
