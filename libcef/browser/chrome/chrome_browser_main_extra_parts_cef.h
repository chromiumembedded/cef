// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

#if BUILDFLAG(IS_LINUX)
#include "printing/printing_context_linux.h"

class CefPrintingContextLinuxDelegate;
#endif

// Wrapper that owns and initialize the browser memory-related extra parts.
class ChromeBrowserMainExtraPartsCef : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsCef();

  ChromeBrowserMainExtraPartsCef(const ChromeBrowserMainExtraPartsCef&) =
      delete;
  ChromeBrowserMainExtraPartsCef& operator=(
      const ChromeBrowserMainExtraPartsCef&) = delete;

  ~ChromeBrowserMainExtraPartsCef() override;

  CefRefPtr<CefRequestContextImpl> request_context() const {
    return global_request_context_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner() const {
    return background_task_runner_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner() const {
    return user_visible_task_runner_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner()
      const {
    return user_blocking_task_runner_;
  }

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostProfileInit(Profile* profile, bool is_initial_profile) override;
  void PostBrowserStart() override;
  void PreMainMessageLoopRun() override;
  void ToolkitInitialized() override;

  CefRefPtr<CefRequestContextImpl> global_request_context_;

  // Blocking task runners exposed via CefTaskRunner. For consistency with
  // previous named thread behavior always execute all pending tasks before
  // shutdown (e.g. to make sure critical data is saved to disk).
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner_;

#if BUILDFLAG(IS_LINUX)
  // Owns the printing delegate and print dialog factory registered in
  // ToolkitInitialized(). |printing_delegate_| is declared first so it is
  // destroyed after |print_dialog_factory_|; the destructor clears the global
  // delegate pointer before it is destroyed, and the factory unregisters itself
  // via the PrintDialogLinuxFactory destructor.
  std::unique_ptr<CefPrintingContextLinuxDelegate> printing_delegate_;
  std::unique_ptr<printing::PrintingContextLinux::PrintDialogFactory>
      print_dialog_factory_;
#endif
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_
