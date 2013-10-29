// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_
#define CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_

#include <string>

namespace v8 {
class Context;
template <class T> class Handle;
class Isolate;
}

namespace WebKit {
class WebFrame;
class WebNode;
class WebString;
class WebView;
}

namespace webkit_glue {

bool CanGoBack(WebKit::WebView* view);
bool CanGoForward(WebKit::WebView* view);
void GoBack(WebKit::WebView* view);
void GoForward(WebKit::WebView* view);

// Retrieve the V8 isolate associated with the frame.
v8::Isolate* GetV8Isolate(WebKit::WebFrame* frame);

// Retrieve the V8 context associated with the frame.
v8::Handle<v8::Context> GetV8Context(WebKit::WebFrame* frame);

// Returns the text of the document element.
std::string DumpDocumentText(WebKit::WebFrame* frame);

bool SetNodeValue(WebKit::WebNode& node, const WebKit::WebString& value);

}  // webkit_glue

#endif  // CEF_LIBCEF_RENDERER_WEBKIT_GLUE_H_
