// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_CALLARGS_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_CALLARGS_H_
#pragma once

#include "base/basictypes.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

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

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_CALLARGS_H_
