// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_AGENT_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_AGENT_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgentClient.h"

namespace WebKit {

class WebDevToolsAgent;
class WebView;

}  // namespace WebKit

class BrowserDevToolsCallArgs;
class BrowserDevToolsClient;

class BrowserDevToolsAgent : public WebKit::WebDevToolsAgentClient {
 public:
  BrowserDevToolsAgent();
  virtual ~BrowserDevToolsAgent();

  void SetWebView(WebKit::WebView* web_view);

  // WebDevToolsAgentClient implementation.
  virtual void sendMessageToInspectorFrontend(
      const WebKit::WebString& data);
  virtual int hostIdentifier();
  virtual void runtimePropertyChanged(const WebKit::WebString& name,
                                      const WebKit::WebString& value);

  virtual WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
      createClientMessageLoop();

  void AsyncCall(const BrowserDevToolsCallArgs& args);

  void attach(BrowserDevToolsClient* client);
  void detach();

  bool evaluateInWebInspector(long call_id,  // NOLINT(runtime/int)
                              const std::string& script);

  BrowserDevToolsClient* client() { return dev_tools_client_; }

 private:
  void Call(const BrowserDevToolsCallArgs& args);
  WebKit::WebDevToolsAgent* GetWebAgent();

  base::WeakPtrFactory<BrowserDevToolsAgent> weak_factory_;
  BrowserDevToolsClient* dev_tools_client_;
  int routing_id_;
  WebKit::WebDevToolsAgent* web_dev_tools_agent_;
  WebKit::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDevToolsAgent);
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_AGENT_H_
