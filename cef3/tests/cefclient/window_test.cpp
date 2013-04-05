// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/window_test.h"

#include <algorithm>
#include <string>

#include "include/wrapper/cef_stream_resource_handler.h"

namespace window_test {

namespace {

const char* kMessagePositionName = "WindowTest.Position";
const char* kMessageMinimizeName = "WindowTest.Minimize";
const char* kMessageMaximizeName = "WindowTest.Maximize";
const char* kMessageRestoreName = "WindowTest.Restore";

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
    if (message_name == kMessagePositionName) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      if (args->GetSize() >= 4) {
        int x = args->GetInt(0);
        int y = args->GetInt(1);
        int width = args->GetInt(2);
        int height = args->GetInt(3);
        SetPos(browser->GetHost()->GetWindowHandle(), x, y, width, height);
      }
      return true;
    } else if (message_name == kMessageMinimizeName) {
      Minimize(browser->GetHost()->GetWindowHandle());
      return true;
    } else if (message_name == kMessageMaximizeName) {
      Maximize(browser->GetHost()->GetWindowHandle());
      return true;
    } else if (message_name == kMessageRestoreName) {
      Restore(browser->GetHost()->GetWindowHandle());
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

void ModifyBounds(const CefRect& display, CefRect& window) {
  window.x += display.x;
  window.y += display.y;

  if (window.x < display.x)
    window.x = display.x;
  if (window.y < display.y)
    window.y = display.y;
  if (window.width < 100)
    window.width = 100;
  else if (window.width >= display.width)
    window.width = display.width;
  if (window.height < 100)
    window.height = 100;
  else if (window.height >= display.height)
    window.height = display.height;
  if (window.x + window.width >= display.x + display.width)
    window.x = display.x + display.width - window.width;
  if (window.y + window.height >= display.y + display.height)
    window.y = display.y + display.height - window.height;
}

}  // namespace window_test
