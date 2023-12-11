// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_RUNNER_DELEGATE_
#define CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_RUNNER_DELEGATE_

#include <memory>

#include "include/cef_base.h"
#include "libcef/common/main_runner_delegate.h"
#include "libcef/common/main_runner_handler.h"

class AlloyMainDelegate;

class AlloyMainRunnerDelegate : public CefMainRunnerDelegate {
 public:
  // |runner| and |settings| will be non-nullptr for the main process only, and
  // will outlive this object.
  AlloyMainRunnerDelegate(CefMainRunnerHandler* runner,
                          CefSettings* settings,
                          CefRefPtr<CefApp> application);

  AlloyMainRunnerDelegate(const AlloyMainRunnerDelegate&) = delete;
  AlloyMainRunnerDelegate& operator=(const AlloyMainRunnerDelegate&) = delete;

  ~AlloyMainRunnerDelegate() override;

 protected:
  // CefMainRunnerDelegate overrides.
  content::ContentMainDelegate* GetContentMainDelegate() override;
  void BeforeMainThreadInitialize(const CefMainArgs& args) override;
  void BeforeMainThreadRun(bool multi_threaded_message_loop) override;
  void AfterUIThreadInitialize() override;
  void BeforeUIThreadShutdown() override;
  void AfterUIThreadShutdown() override;
  void BeforeMainThreadShutdown() override;
  void AfterMainThreadShutdown() override;

 private:
  std::unique_ptr<AlloyMainDelegate> main_delegate_;

  CefMainRunnerHandler* const runner_;
  CefSettings* const settings_;
  CefRefPtr<CefApp> application_;
};

#endif  // CEF_LIBCEF_COMMON_ALLOY_ALLOY_MAIN_RUNNER_DELEGATE_
