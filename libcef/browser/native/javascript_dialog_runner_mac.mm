// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/javascript_dialog_runner_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface CefJavaScriptDialogHelper : NSObject <NSAlertDelegate> {
 @private
  base::scoped_nsobject<NSAlert> alert_;
  NSTextField* textField_;  // WEAK; owned by alert_

  // Copies of the fields in CefJavaScriptDialog because they're private.
  CefJavaScriptDialogRunner::DialogClosedCallback callback_;
}

- (id)initHelperWithCallback:
    (CefJavaScriptDialogRunner::DialogClosedCallback)callback;
- (NSAlert*)alert;
- (NSTextField*)textField;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
- (void)cancel;

@end

@implementation CefJavaScriptDialogHelper

- (id)initHelperWithCallback:
    (CefJavaScriptDialogRunner::DialogClosedCallback)callback {
  if (self = [super init])
    callback_ = std::move(callback);

  return self;
}

- (NSAlert*)alert {
  alert_.reset([[NSAlert alloc] init]);
  return alert_;
}

- (NSTextField*)textField {
  textField_ = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  [[textField_ cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [alert_ setAccessoryView:textField_];
  [[alert_ window] setInitialFirstResponder:textField_];
  [textField_ release];

  return textField_;
}

- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  if (returnCode == NSModalResponseStop)
    return;

  bool success = returnCode == NSAlertFirstButtonReturn;
  std::u16string input;
  if (textField_)
    input = base::SysNSStringToUTF16([textField_ stringValue]);

  std::move(callback_).Run(success, input);
}

- (void)cancel {
  [NSApp endSheet:[alert_ window]];
  alert_.reset();
}

@end

CefJavaScriptDialogRunnerMac::CefJavaScriptDialogRunnerMac()
    : weak_ptr_factory_(this) {}

CefJavaScriptDialogRunnerMac::~CefJavaScriptDialogRunnerMac() {
  Cancel();
}

void CefJavaScriptDialogRunnerMac::Run(
    AlloyBrowserHostImpl* browser,
    content::JavaScriptDialogType message_type,
    const std::u16string& display_url,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback) {
  DCHECK(!helper_.get());
  callback_ = std::move(callback);

  bool text_field = message_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT;
  bool one_button = message_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;

  helper_.reset([[CefJavaScriptDialogHelper alloc]
      initHelperWithCallback:base::BindOnce(
                                 &CefJavaScriptDialogRunnerMac::DialogClosed,
                                 weak_ptr_factory_.GetWeakPtr())]);

  // Show the modal dialog.
  NSAlert* alert = [helper_ alert];
  NSTextField* field = nil;
  if (text_field) {
    field = [helper_ textField];
    [field setStringValue:base::SysUTF16ToNSString(default_prompt_text)];
  }
  [alert setDelegate:helper_];
  [alert setInformativeText:base::SysUTF16ToNSString(message_text)];

  std::u16string label;
  switch (message_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      label = u"JavaScript Alert";
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      label = u"JavaScript Prompt";
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      label = u"JavaScript Confirm";
      break;
  }
  if (!display_url.empty())
    label += u" - " + display_url;

  [alert setMessageText:base::SysUTF16ToNSString(label)];

  [alert addButtonWithTitle:@"OK"];
  if (!one_button) {
    NSButton* other = [alert addButtonWithTitle:@"Cancel"];
    [other setKeyEquivalent:@"\e"];
  }

  // Calling beginSheetModalForWindow:nil is wrong API usage. For now work
  // around the "callee requires a non-null argument" error that occurs when
  // building with the 10.11 SDK. See http://crbug.com/383820 for related
  // discussion.
  // We can't use the newer beginSheetModalForWindow:completionHandler: variant
  // because it fails silently when passed a nil argument (see issue #2726).
  id nilArg = nil;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  [alert beginSheetModalForWindow:nilArg  // nil here makes it app-modal
                    modalDelegate:helper_
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:this];
#pragma clang diagnostic pop

  if ([alert accessoryView])
    [[alert window] makeFirstResponder:[alert accessoryView]];
}

void CefJavaScriptDialogRunnerMac::Cancel() {
  if (helper_.get()) {
    [helper_ cancel];
    helper_.reset(nil);
  }
}

void CefJavaScriptDialogRunnerMac::DialogClosed(
    bool success,
    const std::u16string& user_input) {
  helper_.reset(nil);
  std::move(callback_).Run(success, user_input);
}
