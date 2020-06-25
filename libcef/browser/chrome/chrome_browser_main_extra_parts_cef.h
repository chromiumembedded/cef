// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

// Wrapper that owns and initialize the browser memory-related extra parts.
class ChromeBrowserMainExtraPartsCef : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsCef();
  ~ChromeBrowserMainExtraPartsCef() override;

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
  void PostMainMessageLoopRun() override;

  // Blocking task runners exposed via CefTaskRunner. For consistency with
  // previous named thread behavior always execute all pending tasks before
  // shutdown (e.g. to make sure critical data is saved to disk).
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> user_blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsCef);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_MAIN_EXTRA_PARTS_CEF_H_
