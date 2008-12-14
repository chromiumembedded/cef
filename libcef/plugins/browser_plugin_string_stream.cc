// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_plugin_string_stream.h"

namespace NPAPI {

BrowserPluginStringStream::BrowserPluginStringStream(
    BrowserPluginInstance *instance,
    const std::string &url,
    bool notify_needed,
    void *notify_data)
    : BrowserPluginStream(instance, url.c_str(), notify_needed, notify_data) {
}

BrowserPluginStringStream::~BrowserPluginStringStream() {
}

void BrowserPluginStringStream::SendToPlugin(const std::string &data,
                                             const std::string &mime_type) {
  int length = static_cast<int>(data.length());
  if (Open(mime_type, std::string(), length, 0)) {
    // TODO - check if it was not fully sent, and figure out a backup plan.
    int written = Write(data.c_str(), length, 0);
    NPReason reason = written == length ? NPRES_DONE : NPRES_NETWORK_ERR;
    Close(reason);
  }
}

}

