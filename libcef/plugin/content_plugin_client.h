// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_PLUGIN_CONTENT_PLUGIN_CLIENT_H_
#define CEF_LIBCEF_PLUGIN_CONTENT_PLUGIN_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/plugin/content_plugin_client.h"

class CefContentPluginClient : public content::ContentPluginClient {
 public:
  virtual ~CefContentPluginClient();
  virtual void PluginProcessStarted(const string16& plugin_name) OVERRIDE;
};

#endif  // CEF_LIBCEF_PLUGIN_CONTENT_PLUGIN_CLIENT_H_
