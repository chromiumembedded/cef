// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_H_
#define CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_H_
#pragma once

#include "content/public/browser/javascript_dialog_manager.h"

#if defined(OS_MACOSX)
#if __OBJC__
@class CefJavaScriptDialogHelper;
#else
class CefJavaScriptDialogHelper;
#endif  // __OBJC__
#endif  // defined(OS_MACOSX)

class CefJavaScriptDialogManager;

class CefJavaScriptDialog {
 public:
  CefJavaScriptDialog(
      CefJavaScriptDialogManager* creator,
      content::JavaScriptMessageType message_type,
      const base::string16& display_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback);
  ~CefJavaScriptDialog();

  // Called to cancel a dialog mid-flight.
  void Cancel();

  // Activate the dialog.
  void Activate();

 private:
  CefJavaScriptDialogManager* creator_;
  content::JavaScriptDialogManager::DialogClosedCallback callback_;

#if defined(OS_MACOSX)
  CefJavaScriptDialogHelper* helper_;  // owned
#elif defined(OS_WIN)
  content::JavaScriptMessageType message_type_;
  HWND dialog_win_;
  HWND parent_win_;
  base::string16 message_text_;
  base::string16 default_prompt_text_;
  static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam,
                                     LPARAM lparam);

  // Since the message loop we expect to run in isn't going to be nicely
  // calling IsDialogMessage(), we need to hook the wnd proc and call it
  // ourselves. See http://support.microsoft.com/kb/q187988/
  static bool InstallMessageHook();
  static bool UninstallMessageHook();
  static LRESULT CALLBACK GetMsgProc(int code, WPARAM wparam, LPARAM lparam);
  static HHOOK msg_hook_;
  static int msg_hook_user_count_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CefJavaScriptDialog);
};

#endif  // CEF_LIBCEF_BROWSER_JAVASCRIPT_DIALOG_H_
