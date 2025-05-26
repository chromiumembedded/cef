// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <windows.h>

#include <algorithm>
#include <memory>

#include "include/cef_command_line.h"
#include "include/cef_sandbox_win.h"
#include "include/wrapper/cef_certificate_util_win.h"
#include "include/wrapper/cef_library_loader.h"
#include "include/wrapper/cef_util_win.h"
#include "tests/cefclient/browser/main_context_impl.h"
#include "tests/cefclient/browser/main_message_loop_multithreaded_win.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/main_message_loop_external_pump.h"
#include "tests/shared/browser/main_message_loop_std.h"
#include "tests/shared/browser/util_win.h"
#include "tests/shared/common/client_app_other.h"
#include "tests/shared/common/client_switches.h"
#include "tests/shared/renderer/client_app_renderer.h"

namespace client {
namespace {

// Configure code signing requirements. For a code signing example see
// https://github.com/chromiumembedded/cef/issues/3824#issuecomment-2892139995

// TODO(client): Optionally require that the primary certificate match a
// specific thumbprint by setting this value to the SHA1 hash (e.g. 40 character
// upper-case hex-encoded value). If this valus is empty and |kAllowUnsigned| is
// false then any valid signature will be allowed. This is the "Thumbprint"
// output reported by some Windows PowerShell commands. It can also be retrieved
// directly with a PowerShell command like: > (Get-ChildItem
// Cert:\CurrentUser\My -CodeSigningCert)[0].Thumbprint
constexpr char kRequiredThumbprint[] = "";

// TODO(client): Optionally disallow unsigned binaries by setting this value to
// false. This value is disregarded if |kRequiredThumbprint| is specified.
constexpr bool kAllowUnsigned = true;

// TODO(client): Optionally require that all binaries be signed with the same
// primary thumbprint. This value is ignored when |kRequiredThumbprint| is
// specified or if |kAllowUnsigned| is true.
constexpr bool kRequireMatchingThumbprints = false;

static_assert(sizeof(kRequiredThumbprint) == 1 ||
                  sizeof(kRequiredThumbprint) ==
                      cef_certificate_util::kThumbprintLength + 1,
              "invalid size for kRequiredThumbprint");

const char* RequiredThumbprint(std::string* exe_thumbprint) {
  if constexpr (sizeof(kRequiredThumbprint) ==
                cef_certificate_util::kThumbprintLength + 1) {
    return kRequiredThumbprint;
  }

  if (!kAllowUnsigned && kRequireMatchingThumbprints && exe_thumbprint &&
      exe_thumbprint->length() == cef_certificate_util::kThumbprintLength) {
    return exe_thumbprint->c_str();
  }

  return nullptr;
}

bool VerifyCodeSigningAndLoad(CefScopedLibraryLoader& library_loader,
                              cef_version_info_t* version_info) {
  // Enable early logging support (required before libcef is loaded).
  // The *Assert() calls below will output a FATAL error and crash on failure.
  cef::logging::ScopedEarlySupport scoped_logging({});

  if (library_loader.LoadInSubProcessAssert(version_info)) {
    // Running as a sub-process. We may be sandboxed. Nothing more to be done.
    return true;
  }

  std::string exe_thumbprint;

  // Check signatures for the already loaded executable. This may be the
  // bootstrap, or the client executable if not using the bootstrap.
  const std::wstring& exe_path = cef_util::GetExePath();
  cef_certificate_util::ThumbprintsInfo exe_info;
  cef_certificate_util::ValidateCodeSigningAssert(
      exe_path, RequiredThumbprint(nullptr), kAllowUnsigned, &exe_info);
  if (exe_info.IsSignedAndValid()) {
    exe_thumbprint = exe_info.valid_thumbprints[0];
    CHECK_EQ(cef_certificate_util::kThumbprintLength, exe_thumbprint.length());
  }

#if defined(CEF_USE_BOOTSTRAP)
  // Using a separate bootstrap executable that loaded a client DLL. Check
  // signatures for the already loaded client DLL.
  const std::wstring& client_dll_path =
      cef_util::GetModulePath(client::GetCodeModuleHandle());
  cef_certificate_util::ValidateCodeSigningAssert(
      client_dll_path, RequiredThumbprint(&exe_thumbprint), kAllowUnsigned);
#endif  // defined(CEF_USE_BOOTSTRAP)

  // Require libcef.dll in the same directory as the executable.
  auto sep_pos = exe_path.find_last_of(L"/\\");
  CHECK(sep_pos != std::wstring::npos);
  const auto& libcef_dll_path = exe_path.substr(0, sep_pos + 1) + L"libcef.dll";

  // Validate code signing requirements for libcef.dll before loading, and
  // then load.
  return library_loader.LoadInMainAssert(libcef_dll_path.c_str(),
                                         RequiredThumbprint(&exe_thumbprint),
                                         kAllowUnsigned, version_info);
}

int RunMain(HINSTANCE hInstance,
            int nCmdShow,
            void* sandbox_info,
            cef_version_info_t* version_info) {
  CefMainArgs main_args(hInstance);

  // Dynamically load the CEF library after code signing verification.
  CefScopedLibraryLoader library_loader;
  if (!VerifyCodeSigningAndLoad(library_loader, version_info)) {
    // The verification or load failed. We'll crash before reaching this line.
    NOTREACHED();
    return CEF_RESULT_CODE_KILLED;
  }

  // The CEF library (libcef) is loaded at this point.

  // Parse command-line arguments.
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
  command_line->InitFromString(::GetCommandLineW());

  // Create a ClientApp of the correct type.
  CefRefPtr<CefApp> app;
  ClientApp::ProcessType process_type = ClientApp::GetProcessType(command_line);
  if (process_type == ClientApp::BrowserProcess) {
    app = new ClientAppBrowser();
  } else if (process_type == ClientApp::RendererProcess) {
    app = new ClientAppRenderer();
  } else if (process_type == ClientApp::OtherProcess) {
    app = new ClientAppOther();
  }

  // Execute the secondary process, if any.
  int exit_code = CefExecuteProcess(main_args, app, sandbox_info);
  if (exit_code >= 0) {
    return exit_code;
  }

  // Create the main context object.
  auto context = std::make_unique<MainContextImpl>(command_line, true);

  CefSettings settings;

  if (!sandbox_info) {
    settings.no_sandbox = true;
  }

  // Populate the settings based on command line arguments.
  context->PopulateSettings(&settings);

  // Set the ID for the ICON resource that will be loaded from the main
  // executable and used when creating default Chrome windows such as DevTools
  // and Task Manager. Only used with the Chrome runtime.
  settings.chrome_app_icon_id = IDR_MAINFRAME;

  // Create the main message loop object.
  std::unique_ptr<MainMessageLoop> message_loop;
  if (settings.multi_threaded_message_loop) {
    message_loop = std::make_unique<MainMessageLoopMultithreadedWin>();
  } else if (settings.external_message_pump) {
    message_loop = MainMessageLoopExternalPump::Create();
  } else {
    message_loop = std::make_unique<MainMessageLoopStd>();
  }

  // Initialize the CEF browser process. May return false if initialization
  // fails or if early exit is desired (for example, due to process singleton
  // relaunch behavior).
  if (!context->Initialize(main_args, settings, app, sandbox_info)) {
    return CefGetExitCode();
  }

  // Register scheme handlers.
  test_runner::RegisterSchemeHandlers();

  auto window_config = std::make_unique<RootWindowConfig>();
  window_config->always_on_top =
      command_line->HasSwitch(switches::kAlwaysOnTop);
  window_config->with_osr =
      settings.windowless_rendering_enabled ? true : false;

  // Create the first window.
  context->GetRootWindowManager()->CreateRootWindow(std::move(window_config));

  // Run the message loop. This will block until Quit() is called by the
  // RootWindowManager after all windows have been destroyed.
  int result = message_loop->Run();

  // Shut down CEF.
  context->Shutdown();

  // Release objects in reverse order of creation.
  message_loop.reset();
  context.reset();

  return result;
}

}  // namespace
}  // namespace client

#if defined(CEF_USE_BOOTSTRAP)

// Entry point called by bootstrap.exe when built as a DLL.
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE hInstance,
                                    LPTSTR lpCmdLine,
                                    int nCmdShow,
                                    void* sandbox_info,
                                    cef_version_info_t* version_info) {
  return client::RunMain(hInstance, nCmdShow, sandbox_info, version_info);
}

#else  // !defined(CEF_USE_BOOTSTRAP)

// Program entry point function.
int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(ARCH_CPU_32_BITS)
  // Run the main thread on 32-bit Windows using a fiber with the preferred 4MiB
  // stack size. This function must be called at the top of the executable entry
  // point function (`main()` or `wWinMain()`). It is used in combination with
  // the initial stack size of 0.5MiB configured via the `/STACK:0x80000` linker
  // flag on executable targets. This saves significant memory on threads (like
  // those in the Windows thread pool, and others) whose stack size can only be
  // controlled via the linker flag.
  int exit_code = CefRunWinMainWithPreferredStackSize(wWinMain, hInstance,
                                                      lpCmdLine, nCmdShow);
  if (exit_code >= 0) {
    // The fiber has completed so return here.
    return exit_code;
  }
#endif

  void* sandbox_info = nullptr;

#if defined(CEF_USE_SANDBOX)
  // Manage the life span of the sandbox information object. This is necessary
  // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
  CefScopedSandboxInfo scoped_sandbox;
  sandbox_info = scoped_sandbox.sandbox_info();
#endif

  cef_version_info_t version_info = {};
  CEF_POPULATE_VERSION_INFO(&version_info);

  return client::RunMain(hInstance, nCmdShow, sandbox_info, &version_info);
}

#endif  // !defined(CEF_USE_BOOTSTRAP)
