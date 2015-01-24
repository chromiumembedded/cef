// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_MAIN_CONTEXT_IMPL_H_
#define CEF_TESTS_CEFCLIENT_MAIN_CONTEXT_IMPL_H_

#include "include/base/cef_scoped_ptr.h"
#include "include/cef_command_line.h"
#include "cefclient/main_context.h"

namespace client {

// Used to store global context in the browser process.
class MainContextImpl : public MainContext {
 public:
  MainContextImpl(int argc,
                  const char* const* argv);

  // MainContext members.
  std::string GetConsoleLogPath() OVERRIDE;
  std::string GetDownloadPath(const std::string& file_name) OVERRIDE;
  std::string GetAppWorkingDirectory() OVERRIDE;
  std::string GetMainURL() OVERRIDE;
  void PopulateSettings(CefSettings* settings) OVERRIDE;
  void PopulateBrowserSettings(CefBrowserSettings* settings) OVERRIDE;

 private:
  // Allow deletion via scoped_ptr only.
  friend struct base::DefaultDeleter<MainContextImpl>;

  ~MainContextImpl() {}

  CefRefPtr<CefCommandLine> command_line_;
  std::string main_url_;

  DISALLOW_COPY_AND_ASSIGN(MainContextImpl);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_MAIN_CONTEXT_IMPL_H_
