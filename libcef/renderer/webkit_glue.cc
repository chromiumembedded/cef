// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/webkit_glue.h"

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"

#include "config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "bindings/v8/ScriptController.h"
#include "core/history/BackForwardController.h"
#include "core/page/Page.h"
#include "third_party/WebKit/Source/WebKit/chromium/src/WebFrameImpl.h"
#include "third_party/WebKit/Source/WebKit/chromium/src/WebViewImpl.h"
MSVC_POP_WARNING();
#undef LOG

namespace webkit_glue {

bool CanGoBackOrForward(WebKit::WebView* view, int distance) {
  if (!view)
    return false;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
  if (distance == 0)
    return true;
  if (distance > 0 && distance <= impl->page()->backForward()->forwardCount())
    return true;
  if (distance < 0 && -distance <= impl->page()->backForward()->backCount())
    return true;
  return false;
}

void GoBackOrForward(WebKit::WebView* view, int distance) {
  if (!view)
    return;
  WebKit::WebViewImpl* impl = reinterpret_cast<WebKit::WebViewImpl*>(view);
  impl->page()->goBackOrForward(distance);
}

v8::Handle<v8::Context> GetV8Context(WebKit::WebFrame* frame) {
  WebKit::WebFrameImpl* impl = static_cast<WebKit::WebFrameImpl*>(frame);
  return WebCore::ScriptController::mainWorldContext(impl->frame());
}

base::string16 DumpDocumentText(WebKit::WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebKit::WebElement document_element = frame->document().documentElement();
  if (document_element.isNull())
    return base::string16();

  return document_element.innerText();
}

}  // webkit_glue
