// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
// Otherwise there will be compile errors in wtf/MathExtras.h.
#define _USE_MATH_DEFINES

// Defines required to access Blink internals (unwrap WebNode).
#undef BLINK_IMPLEMENTATION
#define BLINK_IMPLEMENTATION 1
#undef INSIDE_BLINK
#define INSIDE_BLINK 1

#include "libcef/renderer/webkit_glue.h"

#include "base/compiler_specific.h"
#include "v8/include/v8.h"

#include "config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/ScriptController.h"
#include "third_party/WebKit/Source/core/dom/Node.h"
#include "third_party/WebKit/Source/web/WebFrameImpl.h"
#include "third_party/WebKit/Source/web/WebViewImpl.h"
#include "third_party/WebKit/Source/wtf/PassRefPtr.h"
MSVC_POP_WARNING();
#undef LOG

namespace webkit_glue {

bool CanGoBack(WebKit::WebView* view) {
  if (!view)
    return false;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
  return (impl->client()->historyBackListCount() > 0);
}

bool CanGoForward(WebKit::WebView* view) {
  if (!view)
    return false;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
  return (impl->client()->historyForwardListCount() > 0);
}

void GoBack(WebKit::WebView* view) {
  if (!view)
    return;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
  if (impl->client()->historyBackListCount() > 0)
    impl->client()->navigateBackForwardSoon(-1);
}

void GoForward(WebKit::WebView* view) {
  if (!view)
    return;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
 if (impl->client()->historyForwardListCount() > 0)
    impl->client()->navigateBackForwardSoon(1);
}

v8::Isolate* GetV8Isolate(WebKit::WebFrame* frame) {
  WebKit::WebFrameImpl* impl = static_cast<WebKit::WebFrameImpl*>(frame);
  return WebCore::toIsolate(impl->frame());
}

v8::Handle<v8::Context> GetV8Context(WebKit::WebFrame* frame) {
  WebKit::WebFrameImpl* impl = static_cast<WebKit::WebFrameImpl*>(frame);
  return WebCore::ScriptController::mainWorldContext(impl->frame());
}

std::string DumpDocumentText(WebKit::WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebKit::WebElement document_element = frame->document().documentElement();
  if (document_element.isNull())
    return std::string();

  return document_element.innerText().utf8();
}

bool SetNodeValue(WebKit::WebNode& node, const WebKit::WebString& value) {
  WebCore::Node* web_node = node.unwrap<WebCore::Node>();
  web_node->setNodeValue(value);
  return true;
}

}  // webkit_glue
