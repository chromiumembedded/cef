// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include <iostream>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/memory.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_sandbox_win.h"
#include "cef/include/cef_version.h"
#include "cef/include/cef_version_info.h"
#include "cef/include/internal/cef_types.h"
#include "cef/include/wrapper/cef_certificate_util_win.h"
#include "cef/include/wrapper/cef_util_win.h"
#include "cef/libcef/browser/crashpad_runner.h"
#include "cef/libcef/browser/preferred_stack_size_win.inc"
#include "cef/libcef_dll/bootstrap/bootstrap_util_win.h"
#include "cef/libcef_dll/bootstrap/win/resource.h"
#include "chrome/app/delay_load_failure_hook_win.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/win/src/sandbox.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

namespace {

// Sets the current working directory for the process to the directory holding
// the executable if this is the browser process. This avoids leaking a handle
// to an arbitrary directory to child processes (e.g., the crashpad handler
// process).
void SetCwdForBrowserProcess() {
  if (!::IsBrowserProcess()) {
    return;
  }

  std::array<wchar_t, MAX_PATH + 1> buffer;
  buffer[0] = L'\0';
  DWORD length = ::GetModuleFileName(nullptr, &buffer[0], buffer.size());
  if (!length || length >= buffer.size()) {
    return;
  }

  base::SetCurrentDirectory(
      base::FilePath(base::FilePath::StringViewType(&buffer[0], length))
          .DirName());
}

#if DCHECK_IS_ON()
// Displays a message to the user with the error message. Used for fatal
// messages, where we close the app simultaneously. This is for developers only;
// we don't use this in circumstances (like release builds) where users could
// see it, since users don't understand these messages anyway.

// Load a string from the string table in bootstrap.rc.
std::wstring LoadString(int string_id) {
  const int kMaxSize = 100;
  TCHAR buff[kMaxSize] = {0};
  ::LoadString(::GetModuleHandle(nullptr), string_id, buff, kMaxSize);
  return buff;
}

// Replace $1-$2-$3..$9 in the format string with values from |subst|.
// Additionally, any number of consecutive '$' characters is replaced by that
// number less one. Eg $$->$, $$$->$$, etc. Supports up to 9 replacements.
std::wstring FormatErrorString(int string_id,
                               base::span<const std::u16string> subst) {
  return base::UTF16ToWide(base::ReplaceStringPlaceholders(
      base::WideToUTF16(LoadString(string_id)), subst, nullptr));
}

void ShowError(const std::wstring& error) {
  const auto subst = std::to_array<std::u16string>(
      {base::WideToUTF16(bootstrap_util::GetExePath().BaseName().value())});
  const auto& title = FormatErrorString(IDS_ERROR_TITLE, subst);
  const auto& extra_info = LoadString(IDS_ERROR_EXTRA_INFO);

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  std::wcerr << title.c_str() << ": " << error << extra_info;
#else
  if (!::IsDebuggerPresent()) {
    // Displaying a dialog is unnecessary when debugging and can complicate
    // debugging.
    const std::wstring& msg = error + extra_info;
    ::MessageBox(nullptr, msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
  }
#endif
}

#endif  // DCHECK_IS_ON()

std::wstring NormalizeError(const std::wstring& err) {
  std::wstring str = err;
  // Replace newlines.
  std::replace(str.begin(), str.end(), L'\n', L' ');
  return str;
}

// Verify DLL code signing requirements.
void CheckDllCodeSigning(
    const base::FilePath& dll_path,
    const cef_certificate_util::ThumbprintsInfo& exe_thumbprints) {
  cef_certificate_util::ThumbprintsInfo dll_thumbprints;
  cef_certificate_util::GetClientThumbprints(
      dll_path.value(), /*verify_binary=*/true, dll_thumbprints);

  // The DLL and EXE must either both be unsigned or both have all valid
  // signatures and the same primary thumbprint.
  if (!dll_thumbprints.IsSame(exe_thumbprints, /*allow_unsigned=*/true)) {
    // Some part of the certificate validation process failed.
#if DCHECK_IS_ON()
    const auto subst = std::to_array<std::u16string>(
        {base::WideToUTF16(dll_path.BaseName().value()),
         base::WideToUTF16(dll_thumbprints.errors)});
    ShowError(FormatErrorString(IDS_ERROR_INVALID_CERT, subst));
#endif
    if (dll_thumbprints.errors.empty()) {
      LOG(FATAL) << "Failed " << dll_path.value()
                 << " certificate requirements";
    } else {
      LOG(FATAL) << "Failed " << dll_path.value() << " certificate checks: "
                 << NormalizeError(dll_thumbprints.errors);
    }
  }
}

}  // namespace

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
int main(int argc, char* argv[]) {
#else   // !defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
// Entry point function for all processes.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
#endif  // !defined(CEF_BUILD_BOOTSTRAP_CONSOLE)

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  int exit_code = CefRunMainWithPreferredStackSize(main, argc, argv);
#else
  int exit_code = CefRunWinMainWithPreferredStackSize(wWinMain, hInstance,
                                                      lpCmdLine, nCmdShow);
#endif
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  SetCwdForBrowserProcess();
  install_static::InitializeFromPrimaryModule();
  SignalInitializeCrashReporting();
  if (IsBrowserProcess()) {
    chrome::DisableDelayLoadFailureHooksForMainExecutable();
  }

  // Done here to ensure that OOMs that happen early in process initialization
  // are correctly signaled to the OS.
  base::EnableTerminationOnOutOfMemory();
  logging::RegisterAbslAbortHook();

  // Parse command-line arguments.
  const base::CommandLine command_line =
      base::CommandLine::FromString(::GetCommandLineW());

  constexpr char kProcessType[] = "type";
  const bool is_subprocess = command_line.HasSwitch(kProcessType);
  const std::string& process_type =
      command_line.GetSwitchValueASCII(kProcessType);
  if (is_subprocess && process_type.empty()) {
    // Early exit on invalid process type.
    return CEF_RESULT_CODE_BAD_PROCESS_TYPE;
  }

  // Run the crashpad handler now instead of waiting for libcef to load.
  constexpr char kCrashpadHandler[] = "crashpad-handler";
  if (process_type == kCrashpadHandler) {
    return crashpad_runner::RunAsCrashpadHandler(command_line);
  }

  // IsUnsandboxedSandboxType() can't be used here because its result can be
  // gated behind a feature flag, which are not yet initialized.
  // Match the logic in MainDllLoader::Launch.
  const bool is_sandboxed =
      sandbox::policy::SandboxTypeFromCommandLine(command_line) !=
      sandbox::mojom::Sandbox::kNoSandbox;

  std::wstring dll_name;
  base::FilePath exe_path;
  cef_certificate_util::ThumbprintsInfo exe_thumbprints;

  if (is_sandboxed) {
    // Running as a sandboxed sub-process. May already be locked down, so we
    // can't call WinAPI functions. The command-line will already have been
    // validated in ChromeContentBrowserClientCef::
    // AppendExtraCommandLineSwitches. Retrieve the module value without
    // additional validation.
    dll_name = bootstrap_util::GetModuleValue(command_line);
    if (dll_name.empty()) {
      // Default to the command-line program name without extension.
      dll_name = command_line.GetProgram().BaseName().RemoveExtension().value();
    }
  } else {
    // Running as the main process or unsandboxed sub-process.
    exe_path = bootstrap_util::GetExePath();

    // Retrieve the module name with validation.
    dll_name = bootstrap_util::GetValidatedModuleValue(command_line, exe_path);
    if (dll_name.empty()) {
      // Default to the executable module file name without extension. This is
      // safer than relying on the command-line program name.
      dll_name = bootstrap_util::GetDefaultModuleValue(exe_path);
    }

    if (bootstrap_util::IsDefaultExeName(dll_name)) {
#if DCHECK_IS_ON()
      ShowError(LoadString(IDS_ERROR_NO_MODULE_NAME));
#endif
      LOG(FATAL) << "Missing module name";
    }

    cef_certificate_util::GetClientThumbprints(
        exe_path.value(), /*verify_binary=*/true, exe_thumbprints);

    // The executable must either be unsigned or have all valid signatures.
    if (!exe_thumbprints.IsUnsignedOrValid()) {
      // Some part of the certificate validation process failed.
#if DCHECK_IS_ON()
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(exe_path.BaseName().value()),
           base::WideToUTF16(exe_thumbprints.errors)});
      ShowError(FormatErrorString(IDS_ERROR_INVALID_CERT, subst));
#endif
      if (exe_thumbprints.errors.empty()) {
        LOG(FATAL) << "Failed " << exe_path.value()
                   << " certificate requirements";
      } else {
        LOG(FATAL) << "Failed " << exe_path.value() << " certificate checks: "
                   << NormalizeError(exe_thumbprints.errors);
      }
    }
  }

  if (!is_sandboxed) {
    // Check chrome_elf.dll which should be preloaded to support crash
    // reporting.
    if (HMODULE hModule = ::LoadLibrary(L"chrome_elf")) {
      const auto& dll_path = bootstrap_util::GetModulePath(hModule);

      // Must be in the same directory as the EXE.
      if (dll_path.DirName() != exe_path.DirName()) {
#if DCHECK_IS_ON()
        const auto subst = std::to_array<std::u16string>({u"chrome_elf"});
        ShowError(FormatErrorString(IDS_ERROR_INVALID_LOCATION, subst));
#endif
        LOG(FATAL) << "Invalid location: " << dll_path.value();
      }

      CheckDllCodeSigning(dll_path, exe_thumbprints);

      FreeLibrary(hModule);
    } else {
      LOG(FATAL) << "Failed to load chrome_elf.dll with error "
                 << ::GetLastError();
    }

    // Load the client DLL as untrusted (e.g. without executing DllMain or
    // loading additional modules) so that we can first check requirements.
    // LoadLibrary's "default search order" is tricky and we don't want to
    // guess about what DLL it will load. DONT_RESOLVE_DLL_REFERENCES is the
    // only option that doesn't execute DllMain while still allowing us
    // retrieve the path using GetModuleFileName. No execution of the DLL
    // should be attempted while loaded in this mode.
    if (HMODULE hModule = ::LoadLibraryEx(dll_name.c_str(), nullptr,
                                          DONT_RESOLVE_DLL_REFERENCES)) {
      const auto& dll_path = bootstrap_util::GetModulePath(hModule);

      if (!bootstrap_util::IsModulePathAllowed(dll_path, exe_path)) {
#if DCHECK_IS_ON()
        const auto subst =
            std::to_array<std::u16string>({base::WideToUTF16(dll_name)});
        ShowError(FormatErrorString(IDS_ERROR_INVALID_LOCATION, subst));
#endif
        LOG(FATAL) << "Invalid location: " << dll_path.value();
      }

      CheckDllCodeSigning(dll_path, exe_thumbprints);

      FreeLibrary(hModule);
    } else {
#if DCHECK_IS_ON()
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(dll_name),
           base::WideToUTF16(cef_util::GetLastErrorAsString())});
      ShowError(FormatErrorString(IDS_ERROR_LOAD_FAILED, subst));
#endif
      LOG(FATAL) << "Failed to load " << dll_name << ".dll with error "
                 << ::GetLastError();
    }
  }

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  constexpr char kProcName[] = "RunConsoleMain";
  using kProcType = decltype(&RunConsoleMain);
#else
  constexpr char kProcName[] = "RunWinMain";
  using kProcType = decltype(&RunWinMain);
#endif

  // Load the client DLL normally.
  if (HMODULE hModule = ::LoadLibrary(dll_name.c_str())) {
    if (auto* pFunc = (kProcType)::GetProcAddress(hModule, kProcName)) {
      // Initialize the sandbox services.
      // Match the logic in MainDllLoader::Launch.
      sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
      if (!is_subprocess || is_sandboxed) {
        // For child processes that are running as --no-sandbox, don't
        // initialize the sandbox info, otherwise they'll be treated as brokers
        // (as if they were the browser).
        content::InitializeSandboxInfo(
            &sandbox_info, IsExtensionPointDisableSet()
                               ? sandbox::MITIGATION_EXTENSION_POINT_DISABLE
                               : 0);
      }

      cef_version_info_t version_info = {};
      CEF_POPULATE_VERSION_INFO(&version_info);

      // Return immediately without calling FreeLibrary() to avoid an illegal
      // access during shutdown. The sandbox broker owns objects created inside
      // libcef.dll (SandboxWin::InitBrokerServices) and cleanup is triggered
      // via an _onexit handler (SingletonBase::OnExit) called after wWinMain
      // exits.
#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
      return pFunc(argc, argv, &sandbox_info, &version_info);
#else
      return pFunc(hInstance, lpCmdLine, nCmdShow, &sandbox_info,
                   &version_info);
#endif
    } else {
#if DCHECK_IS_ON()
      if (!is_sandboxed) {
        const auto subst = std::to_array<std::u16string>(
            {base::WideToUTF16(dll_name),
             base::WideToUTF16(cef_util::GetLastErrorAsString()),
             base::ASCIIToUTF16(std::string(kProcName))});
        ShowError(FormatErrorString(IDS_ERROR_NO_PROC_EXPORT, subst));
      }
#endif

      LOG(FATAL) << "Failed to find " << kProcName << " in " << dll_name
                 << ".dll with error " << ::GetLastError();
    }
  } else {
#if DCHECK_IS_ON()
    if (!is_sandboxed) {
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(dll_name),
           base::WideToUTF16(cef_util::GetLastErrorAsString())});
      ShowError(FormatErrorString(IDS_ERROR_LOAD_FAILED, subst));
    }
#endif

    LOG(FATAL) << "Failed to load " << dll_name << ".dll with error "
               << ::GetLastError();
  }

  // LOG(FATAL) is [[noreturn]], so we never reach this point.
  NOTREACHED();
}

// Exported by bootstrap.exe and called by the client dll via cef_logging.cc.
// Keep the implementation synchronized with base/logging.cc.
extern "C" __declspec(dllexport) void SetLogFatalCrashKey(const char* file,
                                                          int line,
                                                          const char* message) {
  // In case of an out-of-memory condition, this code could be reentered when
  // constructing and storing the key. Using a static is not thread-safe, but if
  // multiple threads are in the process of a fatal crash at the same time, this
  // should work.
  static bool guarded = false;
  if (guarded) {
    return;
  }

  base::AutoReset<bool> guard(&guarded, true);

  // Only log last path component.
  if (file) {
    const char* slash = UNSAFE_TODO(strrchr(file, '\\'));
    if (slash) {
      file = UNSAFE_TODO(slash + 1);
    }
  }

  auto value = base::StringPrintf("%s:%d: %s", file, line, message);
  if (value.back() == '\n') {
    value.pop_back();
  }

  // Note that we intentionally use LOG_FATAL here (old name for LOGGING_FATAL)
  // as that's understood and used by the crash backend.
  // Using the Crashpad API directly here because base::debug::*CrashKeyString()
  // doesn't appear to work prior to Chromium initialization.
  static crashpad::StringAnnotation<1024> log_fatal("LOG_FATAL");
  log_fatal.Set(value);
}
