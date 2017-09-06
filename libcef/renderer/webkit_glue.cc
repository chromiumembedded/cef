// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/webkit_glue.h"

#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebViewClient.h"

#include "third_party/WebKit/Source/bindings/core/v8/ScriptController.h"
#include "third_party/WebKit/Source/bindings/core/v8/ScriptSourceCode.h"
#include "third_party/WebKit/Source/core/dom/Document.h"
#include "third_party/WebKit/Source/core/dom/Element.h"
#include "third_party/WebKit/Source/core/dom/Node.h"
#include "third_party/WebKit/Source/core/editing/serializers/Serialization.h"
#include "third_party/WebKit/Source/core/exported/WebViewImpl.h"
#include "third_party/WebKit/Source/core/frame/LocalFrame.h"
#include "third_party/WebKit/Source/core/frame/Settings.h"
#include "third_party/WebKit/Source/core/frame/WebLocalFrameImpl.h"
#include "third_party/WebKit/Source/platform/ScriptForbiddenScope.h"
#include "third_party/WebKit/Source/platform/bindings/V8Binding.h"
#include "third_party/WebKit/Source/platform/weborigin/SchemeRegistry.h"
MSVC_POP_WARNING();
#undef LOG

#include "base/logging.h"

namespace webkit_glue {

const int64_t kInvalidFrameId = -1;

bool CanGoBack(blink::WebView* view) {
  if (!view)
    return false;
  blink::WebViewImpl* impl = reinterpret_cast<blink::WebViewImpl*>(view);
  return (impl->Client()->HistoryBackListCount() > 0);
}

bool CanGoForward(blink::WebView* view) {
  if (!view)
    return false;
  blink::WebViewImpl* impl = reinterpret_cast<blink::WebViewImpl*>(view);
  return (impl->Client()->HistoryForwardListCount() > 0);
}

void GoBack(blink::WebView* view) {
  if (!view)
    return;
  blink::WebViewImpl* impl = reinterpret_cast<blink::WebViewImpl*>(view);
  if (impl->Client()->HistoryBackListCount() > 0)
    impl->Client()->NavigateBackForwardSoon(-1);
}

void GoForward(blink::WebView* view) {
  if (!view)
    return;
  blink::WebViewImpl* impl = reinterpret_cast<blink::WebViewImpl*>(view);
  if (impl->Client()->HistoryForwardListCount() > 0)
    impl->Client()->NavigateBackForwardSoon(1);
}

std::string DumpDocumentText(blink::WebLocalFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  blink::WebElement document_element = frame->GetDocument().DocumentElement();
  if (document_element.IsNull())
    return std::string();

  blink::Element* web_element = document_element.Unwrap<blink::Element>();
  return blink::WebString(web_element->innerText()).Utf8();
}

cef_dom_node_type_t GetNodeType(const blink::WebNode& node) {
  const blink::Node* web_node = node.ConstUnwrap<blink::Node>();
  switch (web_node->getNodeType()) {
    case blink::Node::kElementNode:
      return DOM_NODE_TYPE_ELEMENT;
    case blink::Node::kAttributeNode:
      return DOM_NODE_TYPE_ATTRIBUTE;
    case blink::Node::kTextNode:
      return DOM_NODE_TYPE_TEXT;
    case blink::Node::kCdataSectionNode:
      return DOM_NODE_TYPE_CDATA_SECTION;
    case blink::Node::kProcessingInstructionNode:
      return DOM_NODE_TYPE_PROCESSING_INSTRUCTIONS;
    case blink::Node::kCommentNode:
      return DOM_NODE_TYPE_COMMENT;
    case blink::Node::kDocumentNode:
      return DOM_NODE_TYPE_DOCUMENT;
    case blink::Node::kDocumentTypeNode:
      return DOM_NODE_TYPE_DOCUMENT_TYPE;
    case blink::Node::kDocumentFragmentNode:
      return DOM_NODE_TYPE_DOCUMENT_FRAGMENT;
  }
  return DOM_NODE_TYPE_UNSUPPORTED;
}

blink::WebString GetNodeName(const blink::WebNode& node) {
  const blink::Node* web_node = node.ConstUnwrap<blink::Node>();
  return web_node->nodeName();
}

blink::WebString CreateNodeMarkup(const blink::WebNode& node) {
  const blink::Node* web_node = node.ConstUnwrap<blink::Node>();
  return blink::CreateMarkup(web_node);
}

bool SetNodeValue(blink::WebNode& node, const blink::WebString& value) {
  blink::Node* web_node = node.Unwrap<blink::Node>();
  web_node->setNodeValue(value);
  return true;
}

v8::MaybeLocal<v8::Value> CallV8Function(v8::Local<v8::Context> context,
                                         v8::Local<v8::Function> function,
                                         v8::Local<v8::Object> receiver,
                                         int argc,
                                         v8::Local<v8::Value> args[],
                                         v8::Isolate* isolate) {
  v8::MaybeLocal<v8::Value> func_rv;

  // Execute the function call using the V8ScriptRunner so that inspector
  // instrumentation works.
  blink::LocalFrame* frame = blink::ToLocalFrameIfNotDetached(context);
  DCHECK(frame);
  if (frame &&
      frame->GetDocument()->CanExecuteScripts(blink::kAboutToExecuteScript)) {
    func_rv = blink::V8ScriptRunner::CallFunction(
        function, frame->GetDocument(), receiver, argc, args, isolate);
  }

  return func_rv;
}

bool IsTextControlElement(const blink::WebElement& element) {
  const blink::Element* web_element = element.ConstUnwrap<blink::Element>();
  return web_element->IsTextControl();
}

v8::MaybeLocal<v8::Value> ExecuteV8ScriptAndReturnValue(
    const blink::WebString& source,
    const blink::WebString& source_url,
    int start_line,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    v8::TryCatch& tryCatch,
    blink::AccessControlStatus accessControlStatus) {
  // Based on ScriptController::executeScriptAndReturnValue
  DCHECK(isolate);

  if (start_line < 1)
    start_line = 1;

  const blink::KURL kurl =
      source_url.IsEmpty() ? blink::KURL()
                           : blink::KURL(blink::kParsedURLString, source_url);

  const blink::ScriptSourceCode ssc = blink::ScriptSourceCode(
      source, kurl,
      WTF::TextPosition(WTF::OrdinalNumber::FromOneBasedInt(start_line),
                        WTF::OrdinalNumber::FromZeroBasedInt(0)));

  v8::MaybeLocal<v8::Value> result;

  blink::LocalFrame* frame = blink::ToLocalFrameIfNotDetached(context);
  if (!frame)
    return result;

  blink::V8CacheOptions v8CacheOptions(blink::kV8CacheOptionsDefault);
  if (frame && frame->GetSettings())
    v8CacheOptions = frame->GetSettings()->GetV8CacheOptions();

  v8::Local<v8::Script> script;
  if (!blink::V8ScriptRunner::CompileScript(blink::ScriptState::From(context),
                                            ssc, accessControlStatus,
                                            v8CacheOptions)
           .ToLocal(&script)) {
    DCHECK(tryCatch.HasCaught());
    return result;
  }

  return blink::V8ScriptRunner::RunCompiledScript(
      isolate, script, blink::ToExecutionContext(context));
}

bool IsScriptForbidden() {
  return blink::ScriptForbiddenScope::IsScriptForbidden();
}

void RegisterURLSchemeAsLocal(const blink::WebString& scheme) {
  blink::SchemeRegistry::RegisterURLSchemeAsLocal(scheme);
}

void RegisterURLSchemeAsSecure(const blink::WebString& scheme) {
  blink::SchemeRegistry::RegisterURLSchemeAsSecure(scheme);
}

struct CefScriptForbiddenScope::Impl {
  blink::ScriptForbiddenScope scope_;
};

CefScriptForbiddenScope::CefScriptForbiddenScope() : impl_(new Impl()) {}

CefScriptForbiddenScope::~CefScriptForbiddenScope() {}

}  // namespace webkit_glue
