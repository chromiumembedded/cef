// Copyright 2016 The Chromium Embedded Framework Authors. Postions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_SUITE_H_
#define CEF_TESTS_CEFTESTS_TEST_SUITE_H_
#pragma once

#include <string>
#include <vector>

#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

// A single instance of this object will be created by main() in
// run_all_unittests.cc.
class CefTestSuite {
 public:
  CefTestSuite(int argc, char** argv);
  ~CefTestSuite();

  static CefTestSuite* GetInstance();

  void InitMainProcess();
  int Run();

  void GetSettings(CefSettings& settings) const;

  // Register a temp directory that should be deleted on shutdown.
  void RegisterTempDirectory(const CefString& directory);

  // Called after shutdown to delete any registered temp directories.
  void DeleteTempDirectories();

  CefRefPtr<CefCommandLine> command_line() const { return command_line_; }
  CefString root_cache_path() const { return root_cache_path_; }

  // The return value from Run().
  int retval() const { return retval_; }

 private:
  void PreInitialize();
  void Initialize();
  void Shutdown();

  int argc_;
  CefScopedArgArray argv_;
  CefRefPtr<CefCommandLine> command_line_;

  std::vector<CefString> temp_directories_;
  base::Lock temp_directories_lock_;

  CefString root_cache_path_;

  int retval_;
};

#define CEF_SETTINGS_ACCEPT_LANGUAGE "en-GB"

#endif  // CEF_TESTS_CEFTESTS_TEST_SUITE_H_
