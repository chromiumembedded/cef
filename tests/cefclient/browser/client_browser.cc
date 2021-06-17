// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/client_browser.h"
#include "tests/cefclient/browser/main_context.h"

#include "include/cef_command_line.h"
#include "include/cef_crash_util.h"
#include "include/cef_file_util.h"
#include "tests/shared/common/client_switches.h"

namespace client {
namespace browser {

namespace {

class ClientBrowserDelegate : public ClientAppBrowser::Delegate {
 public:
  ClientBrowserDelegate() {}

  void OnContextInitialized(CefRefPtr<ClientAppBrowser> app) override {
    if (CefCrashReportingEnabled()) {
      // Set some crash keys for testing purposes. Keys must be defined in the
      // "crash_reporter.cfg" file. See cef_crash_util.h for details.
      CefSetCrashKeyValue("testkey_small1", "value1_small_browser");
      CefSetCrashKeyValue("testkey_small2", "value2_small_browser");
      CefSetCrashKeyValue("testkey_medium1", "value1_medium_browser");
      CefSetCrashKeyValue("testkey_medium2", "value2_medium_browser");
      CefSetCrashKeyValue("testkey_large1", "value1_large_browser");
      CefSetCrashKeyValue("testkey_large2", "value2_large_browser");
    }

    const std::string& crl_sets_path =
        CefCommandLine::GetGlobalCommandLine()->GetSwitchValue(
            switches::kCRLSetsPath);
    if (!crl_sets_path.empty()) {
      // Load the CRLSets file from the specified path.
      CefLoadCRLSetsFile(crl_sets_path);
    }
  }

  void OnBeforeCommandLineProcessing(
      CefRefPtr<ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Append Chromium command line parameters if touch events are enabled
    if (client::MainContext::Get()->TouchEventsEnabled())
      command_line->AppendSwitchWithValue("touch-events", "enabled");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientBrowserDelegate);
  IMPLEMENT_REFCOUNTING(ClientBrowserDelegate);
};

}  // namespace

void CreateDelegates(ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new ClientBrowserDelegate);
}

}  // namespace browser
}  // namespace client
