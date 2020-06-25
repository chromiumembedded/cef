// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_
#define CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_

#include <memory>

#include "include/cef_base.h"
#include "libcef/common/main_runner_handler.h"
#include "libcef/common/task_runner_manager.h"

#include "base/macros.h"
#include "chrome/app/chrome_main_delegate.h"

class ChromeContentBrowserClientCef;

// CEF override of ChromeMainDelegate
class ChromeMainDelegateCef : public ChromeMainDelegate,
                              public CefTaskRunnerManager {
 public:
  // |runner| will be non-nullptr for the main process only, and will outlive
  // this object.
  explicit ChromeMainDelegateCef(CefMainRunnerHandler* runner);
  ~ChromeMainDelegateCef() override;

  // ChromeMainDelegate overrides.
  void PreCreateMainMessageLoop() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;

 protected:
  // CefTaskRunnerManager overrides.
  scoped_refptr<base::SingleThreadTaskRunner> GetBackgroundTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetUserVisibleTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetUserBlockingTaskRunner()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetRenderTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetWebWorkerTaskRunner() override;

 private:
  ChromeContentBrowserClientCef* content_browser_client() const;

  CefMainRunnerHandler* const runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateCef);
};

#endif  // CEF_LIBCEF_COMMON_CHROME_CHROME_MAIN_DELEGATE_CEF_
