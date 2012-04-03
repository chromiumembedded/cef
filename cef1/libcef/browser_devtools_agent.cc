// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_devtools_agent.h"
#include "libcef/browser_devtools_callargs.h"
#include "libcef/browser_devtools_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "grit/webkit_chromium_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebCString;
using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebView;

namespace {

class WebKitClientMessageLoopImpl
    : public WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop {
 public:
  WebKitClientMessageLoopImpl() : message_loop_(MessageLoop::current()) { }
  virtual ~WebKitClientMessageLoopImpl() {
    message_loop_ = NULL;
  }
  virtual void run() {
    bool old_state = message_loop_->NestableTasksAllowed();
    message_loop_->SetNestableTasksAllowed(true);
    message_loop_->Run();
    message_loop_->SetNestableTasksAllowed(old_state);
  }
  virtual void quitNow() {
    message_loop_->QuitNow();
  }
 private:
  MessageLoop* message_loop_;
};

}  //  namespace

BrowserDevToolsAgent::BrowserDevToolsAgent()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      dev_tools_client_(NULL) {
  static int dev_tools_agent_counter;
  routing_id_ = ++dev_tools_agent_counter;
}

BrowserDevToolsAgent::~BrowserDevToolsAgent() {
}

void BrowserDevToolsAgent::SetWebView(WebKit::WebView* web_view) {
  web_view_ = web_view;
}

void BrowserDevToolsAgent::sendMessageToInspectorFrontend(
       const WebString& data) {
  if (dev_tools_client_)
    dev_tools_client_->AsyncCall(BrowserDevToolsCallArgs(data));
}

int BrowserDevToolsAgent::hostIdentifier() {
  return routing_id_;
}

void BrowserDevToolsAgent::runtimePropertyChanged(
    const WebKit::WebString& name,
    const WebKit::WebString& value) {
  // TODO(cef): Implement.
}

WebKit::WebDevToolsAgentClient::WebKitClientMessageLoop*
    BrowserDevToolsAgent::createClientMessageLoop() {
  return new WebKitClientMessageLoopImpl();
}

void BrowserDevToolsAgent::AsyncCall(const BrowserDevToolsCallArgs &args) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDevToolsAgent::Call, weak_factory_.GetWeakPtr(),
                 args));
}

void BrowserDevToolsAgent::Call(const BrowserDevToolsCallArgs &args) {
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->dispatchOnInspectorBackend(args.data_);
  if (BrowserDevToolsCallArgs::calls_count() == 1 && dev_tools_client_)
    dev_tools_client_->all_messages_processed();
}

WebDevToolsAgent* BrowserDevToolsAgent::GetWebAgent() {
  if (!web_view_)
    return NULL;
  return web_view_->devToolsAgent();
}

void BrowserDevToolsAgent::attach(BrowserDevToolsClient* client) {
  DCHECK(!dev_tools_client_);
  dev_tools_client_ = client;
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->attach();
}

void BrowserDevToolsAgent::detach() {
  DCHECK(dev_tools_client_);
  WebDevToolsAgent* web_agent = GetWebAgent();
  if (web_agent)
    web_agent->detach();
  dev_tools_client_ = NULL;
}

bool BrowserDevToolsAgent::evaluateInWebInspector(
      long call_id,  // NOLINT(runtime/int)
      const std::string& script) {
  WebDevToolsAgent* agent = GetWebAgent();
  if (!agent)
    return false;
  agent->evaluateInWebInspector(call_id, WebString::fromUTF8(script));
  return true;
}
