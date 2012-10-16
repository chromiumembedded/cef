// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/dialog_test.h"
#include "include/cef_browser.h"

#include <string>

namespace dialog_test {

namespace {

const char* kTestUrl = "http://tests/dialogs";
const char* kFileOpenMessageName = "DialogTest.FileOpen";
const char* kFileOpenMultipleMessageName = "DialogTest.FileOpenMultiple";
const char* kFileSaveMessageName = "DialogTest.FileSave";

class RunFileDialogCallback : public CefRunFileDialogCallback {
 public:
  explicit RunFileDialogCallback(const std::string& message_name)
      : message_name_(message_name) {
  }

  virtual void OnFileDialogDismissed(
      CefRefPtr<CefBrowserHost> browser_host,
      const std::vector<CefString>& file_paths) OVERRIDE {
    // Send a message back to the render process with the same name and the list
    // of file paths.
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(message_name_);
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    CefRefPtr<CefListValue> val = CefListValue::Create();
    for (size_t i = 0; i < file_paths.size(); ++i)
      val->SetString(i, file_paths[i]);
    args->SetList(0, val);

    // This will result in a call to the callback registered via JavaScript in
    // dialogs.html.
    browser_host->GetBrowser()->SendProcessMessage(PID_RENDERER, message);
  }

 private:
  std::string message_name_;

  IMPLEMENT_REFCOUNTING(RunFileDialogCallback);
};

// Handle messages in the browser process.
class ProcessMessageDelegate : public ClientHandler::ProcessMessageDelegate {
 public:
  ProcessMessageDelegate() {
  }

  // From ClientHandler::ProcessMessageDelegate.
  virtual bool OnProcessMessageReceived(
      CefRefPtr<ClientHandler> handler,
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    std::string url = browser->GetMainFrame()->GetURL();
    if (url.find(kTestUrl) != 0)
      return false;

    // Sample file type filter.
    std::vector<CefString> file_types;
    file_types.push_back("text/*");
    file_types.push_back(".log");
    file_types.push_back(".patch");

    std::string message_name = message->GetName();
    if (message_name == kFileOpenMessageName) {
      browser->GetHost()->RunFileDialog(FILE_DIALOG_OPEN, "My Open Dialog",
          "test.txt", file_types, new RunFileDialogCallback(message_name));
      return true;
    } else if (message_name == kFileOpenMultipleMessageName) {
      browser->GetHost()->RunFileDialog(FILE_DIALOG_OPEN_MULTIPLE,
          "My Open Multiple Dialog", CefString(), file_types,
          new RunFileDialogCallback(message_name));
      return true;
    } else if (message_name == kFileSaveMessageName) {
      browser->GetHost()->RunFileDialog(FILE_DIALOG_SAVE, "My Save Dialog",
          "test.txt", file_types, new RunFileDialogCallback(message_name));
      return true;
    } else {
      ASSERT(false);  // Not reached.
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(ProcessMessageDelegate);
};

}  // namespace

void CreateProcessMessageDelegates(
    ClientHandler::ProcessMessageDelegateSet& delegates) {
  delegates.insert(new ProcessMessageDelegate);
}

void RunTest(CefRefPtr<CefBrowser> browser) {
  browser->GetMainFrame()->LoadURL(kTestUrl);
}

}  // namespace dialog_test
