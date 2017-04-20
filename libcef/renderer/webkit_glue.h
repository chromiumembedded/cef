// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_
#define CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_

#include <stdint.h>

#include <string>

#include "include/internal/cef_types.h"
#include "third_party/WebKit/Source/platform/loader/fetch/AccessControlStatus.h"
#include "v8/include/v8.h"

namespace blink {
class WebElement;
class WebFrame;
class WebNode;
class WebString;
class WebView;
}

namespace webkit_glue {

extern const int64_t kInvalidFrameId;

bool CanGoBack(blink::WebView* view);
bool CanGoForward(blink::WebView* view);
void GoBack(blink::WebView* view);
void GoForward(blink::WebView* view);

// Returns the text of the document element.
std::string DumpDocumentText(blink::WebFrame* frame);

// Expose additional actions on WebNode.
cef_dom_node_type_t GetNodeType(const blink::WebNode& node);
blink::WebString GetNodeName(const blink::WebNode& node);
blink::WebString CreateNodeMarkup(const blink::WebNode& node);
bool SetNodeValue(blink::WebNode& node, const blink::WebString& value);

int64_t GetIdentifier(blink::WebFrame* frame);
std::string GetUniqueName(blink::WebFrame* frame);

bool IsTextControlElement(const blink::WebElement& element);

v8::MaybeLocal<v8::Value> CallV8Function(v8::Local<v8::Context> context,
                                         v8::Local<v8::Function> function,
                                         v8::Local<v8::Object> receiver,
                                         int argc,
                                         v8::Local<v8::Value> args[],
                                         v8::Isolate* isolate);

v8::MaybeLocal<v8::Value> ExecuteV8ScriptAndReturnValue(
    const blink::WebString& source,
    const blink::WebString& source_url,
    int start_line,
    v8::Local<v8::Context> context,
    v8::Isolate* isolate,
    v8::TryCatch& tryCatch,
    blink::AccessControlStatus accessControlStatus);

bool IsScriptForbidden();

void RegisterURLSchemeAsLocal(const blink::WebString& scheme);
void RegisterURLSchemeAsSecure(const blink::WebString& scheme);
void RegisterURLSchemeAsCORSEnabled(const blink::WebString& scheme);

}  // webkit_glue

#endif  // CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_
