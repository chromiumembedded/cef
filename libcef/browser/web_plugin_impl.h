// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
#define CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
#pragma once

#include "include/cef_web_plugin.h"
#include "content/public/common/webplugininfo.h"

class CefWebPluginInfoImpl : public CefWebPluginInfo {
 public:
  explicit CefWebPluginInfoImpl(const content::WebPluginInfo& plugin_info);

  CefString GetName() override;
  CefString GetPath() override;
  CefString GetVersion() override;
  CefString GetDescription() override;

 private:
  content::WebPluginInfo plugin_info_;

  IMPLEMENT_REFCOUNTING(CefWebPluginInfoImpl);
};

#endif  // CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
