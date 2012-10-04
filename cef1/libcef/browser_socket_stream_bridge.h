// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SOCKET_STREAM_BRIDGE_H_
#define CEF_LIBCEF_BROWSER_SOCKET_STREAM_BRIDGE_H_
#pragma once

#include "base/basictypes.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace WebKit {
class WebSocketStreamHandle;
}  // namespace WebKit

namespace webkit_glue {
class WebSocketStreamHandleDelegate;
class WebSocketStreamHandleBridge;
}  // namespace webkit_glue

class BrowserSocketStreamBridge {
 public:
  static void InitializeOnIOThread(net::URLRequestContext* request_context);
  static void Cleanup();
  static webkit_glue::WebSocketStreamHandleBridge* Create(
      WebKit::WebSocketStreamHandle* handle,
      webkit_glue::WebSocketStreamHandleDelegate* delegate);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSocketStreamBridge);
};

#endif  // CEF_LIBCEF_BROWSER_SOCKET_STREAM_BRIDGE_H_
