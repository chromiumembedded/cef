// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_WIN_H_
#define CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_WIN_H_
#pragma once

#include <windows.h>

#include "libcef/browser/javascript_dialog_runner.h"

class CefJavaScriptDialogRunnerWin : public CefJavaScriptDialogRunner {
 public:
  CefJavaScriptDialogRunnerWin();
  ~CefJavaScriptDialogRunnerWin() override;

  // CefJavaScriptDialogRunner methods:
  void Run(AlloyBrowserHostImpl* browser,
           content::JavaScriptDialogType message_type,
           const std::u16string& display_url,
           const std::u16string& message_text,
           const std::u16string& default_prompt_text,
           DialogClosedCallback callback) override;
  void Cancel() override;

 private:
  void CloseDialog(bool success, const std::wstring& user_input);

  HWND dialog_win_;
  HWND parent_win_;

  content::JavaScriptDialogType message_type_;
  std::wstring message_text_;
  std::wstring default_prompt_text_;
  DialogClosedCallback callback_;

  bool hook_installed_;

  static INT_PTR CALLBACK DialogProc(HWND dialog,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

  // Since the message loop we expect to run in isn't going to be nicely
  // calling IsDialogMessage(), we need to hook the wnd proc and call it
  // ourselves. See http://support.microsoft.com/kb/q187988/
  static bool InstallMessageHook();
  static bool UninstallMessageHook();
  static LRESULT CALLBACK GetMsgProc(int code, WPARAM wparam, LPARAM lparam);
  static HHOOK msg_hook_;
  static int msg_hook_user_count_;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_JAVASCRIPT_DIALOG_RUNNER_WIN_H_
