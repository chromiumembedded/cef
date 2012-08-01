// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_devtools_client.h"
#include "libcef/browser_devtools_agent.h"
#include "libcef/browser_devtools_callargs.h"
#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsAgent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"

using WebKit::WebDevToolsAgent;
using WebKit::WebDevToolsFrontend;
using WebKit::WebDevToolsMessageData;
using WebKit::WebString;
using WebKit::WebView;

BrowserDevToolsClient::BrowserDevToolsClient(CefBrowserImpl* browser,
                                             BrowserDevToolsAgent* agent)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      browser_(browser),
      dev_tools_agent_(agent),
      web_view_(browser->UIT_GetWebView()) {
  web_tools_frontend_.reset(WebDevToolsFrontend::create(web_view_, this,
      WebString::fromUTF8(_Context->locale())));
  dev_tools_agent_->attach(this);
}

BrowserDevToolsClient::~BrowserDevToolsClient() {
  // It is a chance that page will be destroyed at detach step of
  // dev_tools_agent_ and we should clean pending requests a bit earlier.
  weak_factory_.InvalidateWeakPtrs();
  if (dev_tools_agent_)
    dev_tools_agent_->detach();
}

void BrowserDevToolsClient::sendMessageToBackend(
     const WebString& data) {
  if (dev_tools_agent_)
    dev_tools_agent_->AsyncCall(BrowserDevToolsCallArgs(data));
}

void BrowserDevToolsClient::activateWindow() {
  NOTIMPLEMENTED();
}

void BrowserDevToolsClient::closeWindow() {
  NOTIMPLEMENTED();
}

void BrowserDevToolsClient::dockWindow() {
  NOTIMPLEMENTED();
}

void BrowserDevToolsClient::undockWindow() {
  NOTIMPLEMENTED();
}

void BrowserDevToolsClient::AsyncCall(const BrowserDevToolsCallArgs &args) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDevToolsClient::Call, weak_factory_.GetWeakPtr(),
                 args));
}

void BrowserDevToolsClient::Call(const BrowserDevToolsCallArgs &args) {
  web_tools_frontend_->dispatchOnInspectorFrontend(args.data_);
  if (BrowserDevToolsCallArgs::calls_count() == 1)
    all_messages_processed();
}

void BrowserDevToolsClient::all_messages_processed() {
  web_view_->mainFrame()->executeScript(WebKit::WebScriptSource(
      WebString::fromUTF8("if (window.WebInspector && "
      "WebInspector.queuesAreEmpty) WebInspector.queuesAreEmpty();")));
}
