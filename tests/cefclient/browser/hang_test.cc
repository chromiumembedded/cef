// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/hang_test.h"

#include <string>

#include "tests/cefclient/browser/base_client_handler.h"
#include "tests/cefclient/browser/test_runner.h"

namespace client::hang_test {

namespace {

const char kTestUrlPath[] = "/hang";
const char kTestMessageName[] = "HangTest";

// Handle messages in the browser process.
class Handler : public CefMessageRouterBrowserSide::Handler {
 public:
  Handler() = default;

  // Called due to cefQuery execution in hang.html.
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!test_runner::IsTestURL(url, kTestUrlPath)) {
      return false;
    }

    if (auto client_handler = BaseClientHandler::GetForBrowser(browser)) {
      using HangAction = BaseClientHandler::HangAction;

      const std::string& message_name = request;
      if (message_name.find(kTestMessageName) == 0) {
        const std::string& command =
            message_name.substr(sizeof(kTestMessageName));
        if (command == "getcommand") {
          std::string current;
          switch (client_handler->GetHangAction()) {
            case HangAction::kDefault:
              current = "default";
              break;
            case HangAction::kWait:
              current = "wait";
              break;
            case HangAction::kTerminate:
              current = "terminate";
              break;
          }
          callback->Success(current);
        } else if (command == "setdefault") {
          client_handler->SetHangAction(HangAction::kDefault);
        } else if (command == "setwait") {
          client_handler->SetHangAction(HangAction::kWait);
        } else if (command == "setterminate") {
          client_handler->SetHangAction(HangAction::kTerminate);
        } else {
          LOG(ERROR) << "Unrecognized command: " << command;
        }
        return true;
      }
    }

    return false;
  }
};

}  // namespace

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace client::hang_test
