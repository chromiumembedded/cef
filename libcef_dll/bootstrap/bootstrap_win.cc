// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include <iostream>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/include/cef_sandbox_win.h"
#include "cef/libcef/browser/preferred_stack_size_win.inc"
#include "cef/libcef_dll/bootstrap/bootstrap_util_win.h"
#include "cef/libcef_dll/bootstrap/certificate_util_win.h"
#include "cef/libcef_dll/bootstrap/win/resource.h"

namespace {

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
  const std::wstring& msg = error + extra_info;
  ::MessageBox(nullptr, msg.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
#endif
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

  // Parse command-line arguments.
  const base::CommandLine command_line =
      base::CommandLine::FromString(::GetCommandLineW());

  // True if this is a sandboxed sub-process. Uses similar logic to
  // Sandbox::IsProcessSandboxed.
  const bool is_sandboxed =
      command_line.HasSwitch("type") &&
      base::GetCurrentProcessIntegrityLevel() < base::MEDIUM_INTEGRITY;

  std::wstring dll_name;
  base::FilePath exe_path;
  certificate_util::ThumbprintsInfo exe_thumbprints;

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
      ShowError(LoadString(IDS_ERROR_NO_MODULE_NAME));
      return 1;
    }

    certificate_util::GetClientThumbprints(
        exe_path.value(), /*verify_binary=*/true, exe_thumbprints);

    // The executable must either be unsigned or have all valid signatures.
    if (!exe_thumbprints.IsUnsignedOrValid()) {
      // Some part of the certificate validation process failed.
      const auto subst = std::to_array<std::u16string>(
          {base::WideToUTF16(exe_path.BaseName().value()),
           base::WideToUTF16(exe_thumbprints.errors)});
      ShowError(FormatErrorString(IDS_ERROR_INVALID_CERT, subst));
      return 1;
    }
  }

  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  void* sandbox_info = scoped_sandbox.sandbox_info();

#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
  constexpr char kProcName[] = "RunConsoleMain";
  using kProcType = decltype(&RunConsoleMain);
#else
  constexpr char kProcName[] = "RunWinMain";
  using kProcType = decltype(&RunWinMain);
#endif

  std::wstring error;

  if (HMODULE hModule = ::LoadLibrary(dll_name.c_str())) {
    if (!is_sandboxed) {
      const auto& dll_path = bootstrap_util::GetModulePath(hModule);

      if (!bootstrap_util::IsModulePathAllowed(dll_path, exe_path)) {
        const auto subst =
            std::to_array<std::u16string>({base::WideToUTF16(dll_name)});
        error = FormatErrorString(IDS_ERROR_INVALID_LOCATION, subst);
      }

      if (error.empty()) {
        certificate_util::ThumbprintsInfo dll_thumbprints;
        certificate_util::GetClientThumbprints(
            dll_path.value(), /*verify_binary=*/true, dll_thumbprints);

        // The DLL and EXE must either both be unsigned or both have all valid
        // signatures and the same primary thumbprint.
        if (!dll_thumbprints.IsSame(exe_thumbprints, /*allow_unsigned=*/true)) {
          // Some part of the certificate validation process failed.
          const auto subst = std::to_array<std::u16string>(
              {base::WideToUTF16(dll_name + TEXT(".dll")),
               base::WideToUTF16(dll_thumbprints.errors)});
          error = FormatErrorString(IDS_ERROR_INVALID_CERT, subst);
        }
      }
    }

    if (error.empty()) {
      if (auto* pFunc = (kProcType)::GetProcAddress(hModule, kProcName)) {
#if defined(CEF_BUILD_BOOTSTRAP_CONSOLE)
        return pFunc(argc, argv, sandbox_info);
#else
        return pFunc(hInstance, lpCmdLine, nCmdShow, sandbox_info);
#endif
      } else if (!is_sandboxed) {
        const auto subst = std::to_array<std::u16string>(
            {base::WideToUTF16(dll_name),
             base::WideToUTF16(bootstrap_util::GetLastErrorAsString()),
             base::ASCIIToUTF16(std::string(kProcName))});
        error = FormatErrorString(IDS_ERROR_NO_PROC_EXPORT, subst);
      }
    }

    FreeLibrary(hModule);
  } else if (!is_sandboxed) {
    const auto subst = std::to_array<std::u16string>(
        {base::WideToUTF16(dll_name),
         base::WideToUTF16(bootstrap_util::GetLastErrorAsString())});
    error = FormatErrorString(IDS_ERROR_LOAD_FAILED, subst);
  }

  // Don't try to show errors while sandboxed.
  if (!error.empty() && !is_sandboxed) {
    ShowError(error);
  }

  return 1;
}
