// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/binding_test.h"

#include <algorithm>
#include <string>

#include "include/wrapper/cef_stream_resource_handler.h"

namespace binding_test {

namespace {

const char* kMessageName = "binding_test";

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
    std::string message_name = message->GetName();
    if (message_name == kMessageName) {
      // Handle the message.
      std::string result;

      CefRefPtr<CefListValue> args = message->GetArgumentList();
      if (args->GetSize() > 0 && args->GetType(0) == VTYPE_STRING) {
        // Our result is a reverse of the original message.
        result = args->GetString(0);
        std::reverse(result.begin(), result.end());
      } else {
        result = "Invalid request";
      }

      // Send the result back to the render process.
      CefRefPtr<CefProcessMessage> response =
          CefProcessMessage::Create(kMessageName);
      response->GetArgumentList()->SetString(0, result);
      browser->SendProcessMessage(PID_RENDERER, response);

      return true;
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

}  // namespace binding_test
