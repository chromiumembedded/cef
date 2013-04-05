// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
#define CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
#pragma once

#include "include/cef_web_plugin.h"
#include "webkit/plugins/webplugininfo.h"

class CefWebPluginInfoImpl : public CefWebPluginInfo {
 public:
  explicit CefWebPluginInfoImpl(const webkit::WebPluginInfo& plugin_info);

  virtual CefString GetName() OVERRIDE;
  virtual CefString GetPath() OVERRIDE;
  virtual CefString GetVersion() OVERRIDE;
  virtual CefString GetDescription() OVERRIDE;

 private:
  webkit::WebPluginInfo plugin_info_;

  IMPLEMENT_REFCOUNTING(CefWebPluginInfoImpl);
};

#endif  // CEF_LIBCEF_BROWSER_WEB_PLUGIN_IMPL_H_
