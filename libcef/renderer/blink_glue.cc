// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/blink_glue.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view_client.h"

#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#undef LOG

#include "base/logging.h"

namespace blink_glue {

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

  blink::WebFrame* main_frame = view->MainFrame();
  if (main_frame && main_frame->IsWebLocalFrame()) {
    blink::WebViewImpl* view_impl = reinterpret_cast<blink::WebViewImpl*>(view);
    if (view_impl->Client()->HistoryBackListCount() > 0) {
      main_frame->ToWebLocalFrame()->Client()->NavigateBackForwardSoon(
          -1, true /* has_user_gesture */);
    }
  }
}

void GoForward(blink::WebView* view) {
  if (!view)
    return;

  blink::WebFrame* main_frame = view->MainFrame();
  if (main_frame && main_frame->IsWebLocalFrame()) {
    blink::WebViewImpl* view_impl = reinterpret_cast<blink::WebViewImpl*>(view);
    if (view_impl->Client()->HistoryForwardListCount() > 0) {
      main_frame->ToWebLocalFrame()->Client()->NavigateBackForwardSoon(
          1, true /* has_user_gesture */);
    }
  }
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
    blink::SanitizeScriptErrors sanitizeScriptErrors) {
  // Based on ScriptController::executeScriptAndReturnValue
  DCHECK(isolate);

  if (start_line < 1)
    start_line = 1;

  const blink::ScriptSourceCode ssc = blink::ScriptSourceCode(
      source, blink::ScriptSourceLocationType::kInternal,
      nullptr, /* cache_handler */
      blink::KURL(source_url),
      WTF::TextPosition(WTF::OrdinalNumber::FromOneBasedInt(start_line),
                        WTF::OrdinalNumber::FromZeroBasedInt(0)));

  v8::MaybeLocal<v8::Value> result;

  blink::LocalFrame* frame = blink::ToLocalFrameIfNotDetached(context);
  if (!frame)
    return result;

  blink::V8CacheOptions v8CacheOptions(blink::kV8CacheOptionsDefault);
  if (frame && frame->GetSettings())
    v8CacheOptions = frame->GetSettings()->GetV8CacheOptions();

  // Based on V8ScriptRunner::CompileAndRunInternalScript:
  v8::ScriptCompiler::CompileOptions compile_options;
  blink::V8CodeCache::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      blink::V8CodeCache::GetCompileOptions(v8CacheOptions, ssc);

  // Currently internal scripts don't have cache handlers, so we should not
  // produce cache for them.
  DCHECK_EQ(produce_cache_options,
            blink::V8CodeCache::ProduceCacheOptions::kNoProduceCache);

  v8::Local<v8::Script> script;
  // Use default ReferrerScriptInfo here:
  // - nonce: empty for internal script, and
  // - parser_state: always "not parser inserted" for internal scripts.
  if (!blink::V8ScriptRunner::CompileScript(
           blink::ScriptState::From(context), ssc, sanitizeScriptErrors,
           compile_options, no_cache_reason, blink::ReferrerScriptInfo())
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

void RegisterURLSchemeAsSupportingFetchAPI(const blink::WebString& scheme) {
  blink::SchemeRegistry::RegisterURLSchemeAsSupportingFetchAPI(scheme);
}

struct CefScriptForbiddenScope::Impl {
  blink::ScriptForbiddenScope scope_;
};

CefScriptForbiddenScope::CefScriptForbiddenScope() : impl_(new Impl()) {}

CefScriptForbiddenScope::~CefScriptForbiddenScope() {}

bool ResponseWasCached(const blink::WebURLResponse& response) {
  return response.ToResourceResponse().WasCached();
}

}  // namespace blink_glue
