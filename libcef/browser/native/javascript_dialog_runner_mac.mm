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
@interface CefJavaScriptDialogHelper : NSObject<NSAlertDelegate> {
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
    callback_ = callback;

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
  if (returnCode == NSRunStoppedResponse)
    return;

  bool success = returnCode == NSAlertFirstButtonReturn;
  base::string16 input;
  if (textField_)
    input = base::SysNSStringToUTF16([textField_ stringValue]);

  callback_.Run(success, input);
}

- (void)cancel {
  [NSApp endSheet:[alert_ window]];
  alert_.reset();
}

@end

CefJavaScriptDialogRunnerMac::CefJavaScriptDialogRunnerMac()
    : weak_ptr_factory_(this) {
}

CefJavaScriptDialogRunnerMac::~CefJavaScriptDialogRunnerMac() {
  Cancel();
}

void CefJavaScriptDialogRunnerMac::Run(
    CefBrowserHostImpl* browser,
    content::JavaScriptDialogType message_type,
    const base::string16& display_url,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const DialogClosedCallback& callback) {
  DCHECK(!helper_.get());
  callback_ = callback;

  bool text_field =
      message_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT;
  bool one_button =
      message_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;

  helper_.reset(
      [[CefJavaScriptDialogHelper alloc] initHelperWithCallback:
          base::Bind(&CefJavaScriptDialogRunnerMac::DialogClosed,
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

  base::string16 label;
  switch (message_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      label = base::ASCIIToUTF16("JavaScript Alert");
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      label = base::ASCIIToUTF16("JavaScript Prompt");
      break;
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      label = base::ASCIIToUTF16("JavaScript Confirm");
      break;
  }
  if (!display_url.empty())
    label += base::ASCIIToUTF16(" - ") + display_url;

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
  id nilArg = nil;
  [alert
      beginSheetModalForWindow:nilArg  // nil here makes it app-modal
                 modalDelegate:helper_
                didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                   contextInfo:this];

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
    const base::string16& user_input) {
  helper_.reset(nil);
  callback_.Run(success, user_input);
}
