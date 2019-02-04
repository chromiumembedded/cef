// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_BLINK_GLUE_H_
#define CEF_LIBCEF_RENDERER_BLINK_GLUE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "include/internal/cef_types.h"

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "v8/include/v8.h"

namespace blink {
class WebElement;
class WebLocalFrame;
class WebNode;
class WebString;
class WebURLResponse;
class WebView;
}  // namespace blink

namespace blink_glue {

BLINK_EXPORT extern const int64_t kInvalidFrameId;

BLINK_EXPORT bool CanGoBack(blink::WebView* view);
BLINK_EXPORT bool CanGoForward(blink::WebView* view);
BLINK_EXPORT void GoBack(blink::WebView* view);
BLINK_EXPORT void GoForward(blink::WebView* view);

// Returns the text of the document element.
BLINK_EXPORT std::string DumpDocumentText(blink::WebLocalFrame* frame);

// Expose additional actions on WebNode.
BLINK_EXPORT cef_dom_node_type_t GetNodeType(const blink::WebNode& node);
BLINK_EXPORT blink::WebString GetNodeName(const blink::WebNode& node);
BLINK_EXPORT blink::WebString CreateNodeMarkup(const blink::WebNode& node);
BLINK_EXPORT bool SetNodeValue(blink::WebNode& node,
                               const blink::WebString& value);

BLINK_EXPORT bool IsTextControlElement(const blink::WebElement& element);

BLINK_EXPORT v8::MaybeLocal<v8::Value> CallV8Function(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> function,
    v8::Local<v8::Object> receiver,
    int argc,
    v8::Local<v8::Value> args[],
    v8::Isolate* isolate);

BLINK_EXPORT v8::MaybeLocal<v8::Value> ExecuteV8ScriptAndReturnValue(
    const blink::WebString& source,
    const blink::WebString& source_url,
    int start_line,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    v8::TryCatch& tryCatch,
    blink::SanitizeScriptErrors sanitizeScriptErrors);

BLINK_EXPORT bool IsScriptForbidden();

BLINK_EXPORT void RegisterURLSchemeAsLocal(const blink::WebString& scheme);
BLINK_EXPORT void RegisterURLSchemeAsSecure(const blink::WebString& scheme);

BLINK_EXPORT void RegisterURLSchemeAsSupportingFetchAPI(
    const blink::WebString& scheme);

// Wrapper for blink::ScriptForbiddenScope.
class BLINK_EXPORT CefScriptForbiddenScope final {
 public:
  CefScriptForbiddenScope();
  ~CefScriptForbiddenScope();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(CefScriptForbiddenScope);
};

BLINK_EXPORT bool ResponseWasCached(const blink::WebURLResponse& response);

}  // namespace blink_glue

#endif  // CEF_LIBCEF_RENDERER_BLINK_GLUE_H_
