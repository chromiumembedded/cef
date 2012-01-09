// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_devtools_callargs.h"

#include "base/logging.h"

// static
int BrowserDevToolsCallArgs::calls_count_ = 0;

BrowserDevToolsCallArgs::BrowserDevToolsCallArgs(
    const WebKit::WebString& data)
    : data_(data) {
  ++calls_count_;
}

BrowserDevToolsCallArgs::BrowserDevToolsCallArgs(
    const BrowserDevToolsCallArgs& args)
    : data_(args.data_) {
  ++calls_count_;
}

BrowserDevToolsCallArgs::~BrowserDevToolsCallArgs() {
  --calls_count_;
  DCHECK_GE(calls_count_, 0);
}
