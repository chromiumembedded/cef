// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/client_update_win.h"

#if defined(CEF_USE_BOOTSTRAP)

#include <windows.h>

#include <string>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_util_win.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/browser/util_win.h"

namespace client {

namespace {

// Resource name for embedded installer configuration (must match .rc file).
constexpr const wchar_t* kConfigResourceName = L"CEF_INSTALLER_CONFIG";

// Function pointer type for RunInstaller export from bootstrap.exe.
typedef const char* (*RunInstallerFunc)(const char* command,
                                        const char* config_json);

// Resolve the RunInstaller export from bootstrap.exe (the main executable).
// Returns nullptr if not running under bootstrap or the export is unavailable.
RunInstallerFunc GetRunInstaller() {
  HMODULE bootstrap = GetModuleHandle(nullptr);
  return reinterpret_cast<RunInstallerFunc>(
      GetProcAddress(bootstrap, "RunInstaller"));
}

void BackgroundUpdateCheck(RunInstallerFunc run_installer) {
  // Read embedded config from client DLL resource.
  std::string config_data;
  if (!cef_util::ReadResourceData(GetCodeModuleHandle(), kConfigResourceName,
                                  &config_data)) {
    return;
  }

  // Parse the embedded config JSON.
  CefRefPtr<CefValue> value = CefParseJSON(config_data, JSON_PARSER_RFC);
  if (!value || value->GetType() != VTYPE_DICTIONARY) {
    return;
  }

  // If unchecked_cef_path is set, the app bundles its own CEF and the
  // bootstrap used it directly. Background updates are not applicable.
  CefRefPtr<CefDictionaryValue> config = value->GetDictionary();
  if (config->HasKey("unchecked_cef_path") &&
      config->GetType("unchecked_cef_path") == VTYPE_STRING &&
      !config->GetString("unchecked_cef_path").empty()) {
    return;
  }

  // Add extended fields for background update.
  config->SetBool("background_mode", true);
  config->SetBool("show_progress_ui", false);

  // Serialize back to JSON.
  value->SetDictionary(config);
  CefString config_json = CefWriteJSON(value, JSON_WRITER_DEFAULT);

  // Run update check.
  const char* result_json =
      run_installer("update", config_json.ToString().c_str());
  if (!result_json) {
    return;
  }

  // Parse and handle result.
  CefRefPtr<CefValue> result = CefParseJSON(result_json, JSON_PARSER_RFC);
  if (result && result->GetType() == VTYPE_DICTIONARY) {
    CefRefPtr<CefDictionaryValue> dict = result->GetDictionary();
    if (dict->GetBool("success") && dict->GetString("outcome") == "committed") {
      LOG(INFO) << "Background CEF update completed successfully";
    }
  }
}

void ConfirmLaunchHealthAndCheckForUpdates() {
  RunInstallerFunc run_installer = GetRunInstaller();
  if (!run_installer) {
    // Not running under bootstrap, or export not available.
    return;
  }

  // kLaunchSuccess doesn't need config — the sentinel path is a process-global
  // set by the bootstrap before RunWinMain. Confirming here writes
  // running=false, consecutive_failures=0 to the launch state sentinel so a
  // later app-level crash does not penalize the (working) CEF version.
  const char* confirmation_result = run_installer("launch_success", nullptr);
  if (!confirmation_result) {
    return;
  }

  // RunInstaller result storage is thread-local and reused by the next call
  // on this thread. Copy and parse the confirmation result before invoking the
  // update command.
  const std::string confirmation_copy(confirmation_result);
  CefRefPtr<CefValue> confirmation =
      CefParseJSON(confirmation_copy, JSON_PARSER_RFC);
  if (!confirmation || confirmation->GetType() != VTYPE_DICTIONARY) {
    return;
  }
  CefRefPtr<CefDictionaryValue> result = confirmation->GetDictionary();
  if (!result->HasKey("success") || result->GetType("success") != VTYPE_BOOL ||
      !result->GetBool("success")) {
    return;
  }

  BackgroundUpdateCheck(run_installer);
}

}  // namespace

void StartLaunchHealthConfirmation() {
  // The client DLL's CEF_INSTALLER_CONFIG must opt in with
  // "launch_health": "explicit". Partial adoption is unsafe: every code path
  // that reaches the application-defined healthy point must confirm.
  // Confirm launch health after 60 seconds of uptime. By this point the app
  // has initialized and rendered content, so the CEF version is working; any
  // later crash is most likely app-level rather than a CEF-load failure.
  // Confirm first so the running version is eligible for the installer's
  // existing confirmed-fallback pruning protection. A failed confirmation
  // skips the update for this process without retrying.
  CefPostDelayedTask(TID_FILE_BACKGROUND,
                     base::BindOnce(&ConfirmLaunchHealthAndCheckForUpdates),
                     60000);
}

}  // namespace client

#endif  // defined(CEF_USE_BOOTSTRAP)
