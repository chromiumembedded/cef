// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_SOCKET_STREAM_BRIDGE_H
#define _BROWSER_SOCKET_STREAM_BRIDGE_H

namespace net {
class URLRequestContext;
}

class BrowserSocketStreamBridge {
 public:
  static void InitializeOnIOThread(net::URLRequestContext* request_context);
  static void Cleanup();
};

#endif  // _BROWSER_SOCKET_STREAM_BRIDGE_H
