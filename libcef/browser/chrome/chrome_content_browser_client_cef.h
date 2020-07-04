// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTENT_BROWSER_CLIENT_CEF_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTENT_BROWSER_CLIENT_CEF_

#include <memory>

#include "libcef/browser/request_context_impl.h"

#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client.h"

class ChromeBrowserMainExtraPartsCef;

// CEF override of ChromeContentBrowserClient
class ChromeContentBrowserClientCef : public ChromeContentBrowserClient {
 public:
  explicit ChromeContentBrowserClientCef(StartupData* startup_data = nullptr);
  ~ChromeContentBrowserClientCef() override;

  // ChromeContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;

  CefRefPtr<CefRequestContextImpl> request_context() const;

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner() const;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner() const;

 private:
  ChromeBrowserMainExtraPartsCef* browser_main_parts_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientCef);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_CONTENT_BROWSER_CLIENT_CEF_
