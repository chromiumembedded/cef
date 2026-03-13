// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mock client DLL for E2E tests. Loaded by bootstrap.exe in launcher mode.
// Calls RunInstaller (exported by bootstrap.exe) and writes a marker file
// with the result.
//
// Environment variables used:
//   CEF_E2E_MARKER_PATH      - Path to write the result marker JSON file
//   CEF_E2E_CONFIG_JSON      - Config JSON to pass to RunInstaller
//   CEF_E2E_COMMAND          - Command string (default: "install")
//   CEF_E2E_THREADING_MODE   - "multi" to spawn two threads calling
//                               RunInstaller sequentially on separate threads
//                            - "parallel" to spawn two threads calling
//                               RunInstaller concurrently
//   CEF_E2E_CONFIG_JSON_A    - Config JSON for thread A (multi/parallel mode)
//   CEF_E2E_CONFIG_JSON_B    - Config JSON for thread B (multi/parallel mode)
//   CEF_E2E_LAUNCH_SUCCESS   - When set (with CEF_E2E_EXIT_CODE), call
//                               RunInstaller("launch_success") to confirm
//                               launch health before returning the exit code

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "include/cef_version_info.h"

typedef const char* (*RunInstallerFn)(const char* command,
                                      const char* config_json);

static RunInstallerFn GetRunInstaller() {
  HMODULE exe = ::GetModuleHandleW(nullptr);
  return reinterpret_cast<RunInstallerFn>(
      ::GetProcAddress(exe, "RunInstaller"));
}

static std::string GetEnvVar(const char* name) {
  char buf[8192] = {};
  DWORD len = ::GetEnvironmentVariableA(name, buf, sizeof(buf));
  return len > 0 ? std::string(buf, len) : std::string();
}

static std::string WideToJsonEscaped(const wchar_t* wide) {
  if (!wide) {
    return "";
  }
  int len =
      ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) {
    return "";
  }
  std::string utf8(len - 1, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, &utf8[0], len, nullptr, nullptr);
  std::string escaped;
  escaped.reserve(utf8.size());
  for (char c : utf8) {
    if (c == '\\') {
      escaped += "\\\\";
    } else if (c == '"') {
      escaped += "\\\"";
    } else {
      escaped += c;
    }
  }
  return escaped;
}

static std::string Utf8ToJsonEscaped(const char* value) {
  if (!value) {
    return "";
  }
  std::string escaped;
  for (const char* current = value; *current; ++current) {
    const unsigned char c = static_cast<unsigned char>(*current);
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          escaped += "\\u00";
          escaped += kHex[c >> 4];
          escaped += kHex[c & 0x0F];
        } else {
          escaped += static_cast<char>(c);
        }
    }
  }
  return escaped;
}

static void WriteMarkerFile(const std::string& path,
                            const char* result_json,
                            const cef_version_info_t* vi) {
  FILE* f = fopen(path.c_str(), "w");
  if (!f) {
    return;
  }

  fprintf(f, "{\n");
  fprintf(f, "  \"result\": %s,\n", result_json ? result_json : "null");

  if (vi && vi->size >= CEF_VERSION_INFO_SIZE_WITH_INSTALLER_ERROR) {
    std::string escaped_path = WideToJsonEscaped(vi->libcef_path);
    fprintf(f, "  \"libcef_path\": \"%s\",\n", escaped_path.c_str());
    fprintf(f, "  \"libcef_is_bundled\": %d,\n", vi->libcef_is_bundled);
    fprintf(f, "  \"libcef_version_full\": \"%s\",\n",
            vi->libcef_version_full ? vi->libcef_version_full : "");
    fprintf(f, "  \"cef_version_major\": %d,\n", vi->cef_version_major);
    fprintf(f, "  \"installer_error_code\": %d,\n", vi->installer_error_code);
    if (vi->installer_error_message) {
      const std::string escaped_message =
          Utf8ToJsonEscaped(vi->installer_error_message);
      fprintf(f, "  \"installer_error_message\": \"%s\"\n",
              escaped_message.c_str());
    } else {
      fprintf(f, "  \"installer_error_message\": null\n");
    }
  } else {
    fprintf(f, "  \"version_info\": null\n");
  }

  fprintf(f, "}\n");
  fclose(f);
}

// Per-thread context for multi-threaded RunInstaller calls.
struct ThreadContext {
  RunInstallerFn run_installer;
  std::string command;
  std::string config_json;
  std::string result_json;
  bool success;
};

static DWORD WINAPI RunInstallerThread(LPVOID param) {
  auto* ctx = static_cast<ThreadContext*>(param);
  const char* result =
      ctx->run_installer(ctx->command.c_str(), ctx->config_json.c_str());
  ctx->result_json = result ? result : "null";
  ctx->success = result && strstr(result, "\"success\":true");
  return 0;
}

static void WriteMultiMarkerFile(const std::string& path,
                                 const ThreadContext& ctx_a,
                                 const ThreadContext& ctx_b,
                                 const cef_version_info_t* vi) {
  FILE* f = fopen(path.c_str(), "w");
  if (!f) {
    return;
  }
  fprintf(f, "{\n");
  fprintf(f, "  \"result_a\": %s,\n", ctx_a.result_json.c_str());
  fprintf(f, "  \"result_b\": %s,\n", ctx_b.result_json.c_str());
  if (vi) {
    fprintf(f, "  \"cef_version_major\": %d\n", vi->cef_version_major);
  } else {
    fprintf(f, "  \"version_info\": null\n");
  }
  fprintf(f, "}\n");
  fclose(f);
}

static int RunMultiThreaded(const std::string& marker_path,
                            const std::string& command,
                            RunInstallerFn run_installer,
                            const cef_version_info_t* vi,
                            bool parallel) {
  std::string config_a = GetEnvVar("CEF_E2E_CONFIG_JSON_A");
  std::string config_b = GetEnvVar("CEF_E2E_CONFIG_JSON_B");
  if (config_a.empty() || config_b.empty()) {
    WriteMarkerFile(
        marker_path,
        "{\"success\":false,\"error_message\":\"missing config A or B\"}", vi);
    return 1;
  }

  ThreadContext ctx_a = {run_installer, command, config_a, "null", false};
  ThreadContext ctx_b = {run_installer, command, config_b, "null", false};

  HANDLE thread_a =
      ::CreateThread(nullptr, 0, RunInstallerThread, &ctx_a, 0, nullptr);

  if (parallel) {
    // Run both threads concurrently. Tests that ScopedThreadPool's refcount
    // keeps the pool alive while either thread is still using it.
    HANDLE thread_b =
        ::CreateThread(nullptr, 0, RunInstallerThread, &ctx_b, 0, nullptr);
    HANDLE handles[] = {thread_a, thread_b};
    ::WaitForMultipleObjects(2, handles, TRUE, 60000);
    ::CloseHandle(thread_a);
    ::CloseHandle(thread_b);
  } else {
    // Run sequentially on separate threads. The test goal is verifying
    // thread-local result strings don't cross-contaminate.
    ::WaitForSingleObject(thread_a, 30000);
    ::CloseHandle(thread_a);

    HANDLE thread_b =
        ::CreateThread(nullptr, 0, RunInstallerThread, &ctx_b, 0, nullptr);
    ::WaitForSingleObject(thread_b, 30000);
    ::CloseHandle(thread_b);
  }

  WriteMultiMarkerFile(marker_path, ctx_a, ctx_b, vi);
  return (ctx_a.success && ctx_b.success) ? 0 : 3;
}

extern "C" __declspec(dllexport) int RunWinMain(HINSTANCE hInstance,
                                                LPTSTR lpCmdLine,
                                                int nCmdShow,
                                                void* sandbox_info,
                                                cef_version_info_t* vi) {
  std::string marker_path = GetEnvVar("CEF_E2E_MARKER_PATH");
  std::string exit_code_str = GetEnvVar("CEF_E2E_EXIT_CODE");

  // When CEF_E2E_EXIT_CODE is set, skip RunInstaller entirely. Write the
  // marker file with version_info from the bootstrap and return the
  // specified exit code. This lets E2E tests control the exit code to
  // simulate clean exits, crashes, and neutral exits.
  if (!exit_code_str.empty()) {
    int exit_code = atoi(exit_code_str.c_str());

    // When CEF_E2E_LAUNCH_SUCCESS is set, confirm launch health via the
    // RunInstaller export before returning the exit code. This composes with
    // CEF_E2E_EXIT_CODE: a confirm-then-crash run leaves the sentinel
    // confirmed (running=false, consecutive_failures=0) before the crash exit,
    // so the crash does not penalize the CEF version. Mirrors cefclient's
    // ConfirmLaunchHealth — kLaunchSuccess ignores config (the sentinel path
    // is a process-global the bootstrap set before RunWinMain), so pass null.
    if (!GetEnvVar("CEF_E2E_LAUNCH_SUCCESS").empty()) {
      RunInstallerFn run_installer = GetRunInstaller();
      if (run_installer) {
        run_installer("launch_success", nullptr);
      }
    }

    if (!marker_path.empty()) {
      WriteMarkerFile(marker_path, "{\"success\":true}", vi);
    }
    return exit_code;
  }

  std::string config_json = GetEnvVar("CEF_E2E_CONFIG_JSON");
  std::string command = GetEnvVar("CEF_E2E_COMMAND");
  std::string threading_mode = GetEnvVar("CEF_E2E_THREADING_MODE");
  if (command.empty()) {
    command = "install";
  }

  if (marker_path.empty()) {
    return 1;
  }

  RunInstallerFn run_installer = GetRunInstaller();
  if (!run_installer) {
    WriteMarkerFile(
        marker_path,
        "{\"success\":false,\"error_message\":\"RunInstaller not found\"}", vi);
    return 2;
  }

  if (threading_mode == "multi" || threading_mode == "parallel") {
    return RunMultiThreaded(marker_path, command, run_installer, vi,
                            threading_mode == "parallel");
  }

  // Single-threaded mode (default).
  if (config_json.empty()) {
    WriteMarkerFile(
        marker_path,
        "{\"success\":false,\"error_message\":\"no config or marker path\"}",
        vi);
    return 1;
  }

  const char* result = run_installer(command.c_str(), config_json.c_str());
  WriteMarkerFile(marker_path, result, vi);

  if (result && strstr(result, "\"success\":true")) {
    return 0;
  }
  return 3;
}

BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  return TRUE;
}
