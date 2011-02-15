// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_DEVTOOLS_CALLARGS_H
#define _BROWSER_DEVTOOLS_CALLARGS_H

#include "base/basictypes.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

class BrowserDevToolsCallArgs {
 public:
  explicit BrowserDevToolsCallArgs(const WebKit::WebString& data);

  BrowserDevToolsCallArgs(const BrowserDevToolsCallArgs& args);

  ~BrowserDevToolsCallArgs();

  static int calls_count() { return calls_count_; }

  WebKit::WebString data_;

 private:
  static int calls_count_;
};

#endif  // _BROWSER_DEVTOOLS_CALLARGS_H
