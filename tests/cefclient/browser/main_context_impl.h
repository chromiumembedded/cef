// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_MAIN_CONTEXT_IMPL_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_MAIN_CONTEXT_IMPL_H_
#pragma once

#include <memory>

#include "include/base/cef_thread_checker.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"

namespace client {

// Used to store global context in the browser process.
class MainContextImpl : public MainContext {
 public:
  MainContextImpl(CefRefPtr<CefCommandLine> command_line,
                  bool terminate_when_all_windows_closed);

  // MainContext members.
  std::string GetConsoleLogPath() override;
  std::string GetDownloadPath(const std::string& file_name) override;
  std::string GetAppWorkingDirectory() override;
  std::string GetMainURL() override;
  cef_color_t GetBackgroundColor() override;
  bool UseChromeRuntime() override;
  bool UseViews() override;
  bool UseWindowlessRendering() override;
  bool TouchEventsEnabled() override;
  void PopulateSettings(CefSettings* settings) override;
  void PopulateBrowserSettings(CefBrowserSettings* settings) override;
  void PopulateOsrSettings(OsrRendererSettings* settings) override;
  RootWindowManager* GetRootWindowManager() override;

  // Initialize CEF and associated main context state. This method must be
  // called on the same thread that created this object.
  bool Initialize(const CefMainArgs& args,
                  const CefSettings& settings,
                  CefRefPtr<CefApp> application,
                  void* windows_sandbox_info);

  // Shut down CEF and associated context state. This method must be called on
  // the same thread that created this object.
  void Shutdown();

 private:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<MainContextImpl>;

  ~MainContextImpl();

  // Returns true if the context is in a valid state (initialized and not yet
  // shut down).
  bool InValidState() const { return initialized_ && !shutdown_; }

  CefRefPtr<CefCommandLine> command_line_;
  const bool terminate_when_all_windows_closed_;

  // Track context state. Accessing these variables from multiple threads is
  // safe because only a single thread will exist at the time that they're set
  // (during context initialization and shutdown).
  bool initialized_;
  bool shutdown_;

  std::string main_url_;
  cef_color_t background_color_;
  cef_color_t browser_background_color_;
  bool use_windowless_rendering_;
  int windowless_frame_rate_;
  bool use_chrome_runtime_;
  bool use_views_;
  bool touch_events_enabled_;

  std::unique_ptr<RootWindowManager> root_window_manager_;

#if defined(OS_WIN)
  bool shared_texture_enabled_;
#endif

  bool external_begin_frame_enabled_;

  // Used to verify that methods are called on the correct thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MainContextImpl);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_MAIN_CONTEXT_IMPL_H_
