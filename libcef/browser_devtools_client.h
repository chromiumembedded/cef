// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_DEVTOOLS_CLIENT_H
#define _BROWSER_DEVTOOLS_CLIENT_H

#include "base/scoped_ptr.h"
#include "base/task.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontendClient.h"

namespace WebKit {

class WebDevToolsFrontend;
struct WebDevToolsMessageData;

} // namespace WebKit

class BrowserDevToolsCallArgs;
class BrowserDevToolsAgent;
class CefBrowserImpl;

class BrowserDevToolsClient: public WebKit::WebDevToolsFrontendClient {

 public:
  BrowserDevToolsClient(CefBrowserImpl* browser,
                        BrowserDevToolsAgent* agent);
  virtual ~BrowserDevToolsClient();

  // WebDevToolsFrontendClient implementation
  virtual void sendFrontendLoaded();
  virtual void sendMessageToBackend(const WebKit::WebString& data);
  virtual void sendDebuggerCommandToAgent(const WebKit::WebString& command);

  virtual void activateWindow();
  virtual void closeWindow();
  virtual void dockWindow();
  virtual void undockWindow();

  void AsyncCall(const BrowserDevToolsCallArgs& args);

  void all_messages_processed();

  CefBrowserImpl* browser() { return browser_; }

 private:
  void Call(const BrowserDevToolsCallArgs& args);

  ScopedRunnableMethodFactory<BrowserDevToolsClient> call_method_factory_;
  CefBrowserImpl* browser_;
  BrowserDevToolsAgent* dev_tools_agent_;
  WebKit::WebView* web_view_;
  scoped_ptr<WebKit::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDevToolsClient);
};

#endif  // _BROWSER_DEVTOOLS_CLIENT_H
