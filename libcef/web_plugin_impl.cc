// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

class CefWebPluginInfoImpl : public CefWebPluginInfo {
 public:
  explicit CefWebPluginInfoImpl(const webkit::WebPluginInfo& plugin_info)
      : plugin_info_(plugin_info) {
  }

  virtual CefString GetName() OVERRIDE {
    return plugin_info_.name;
  }

  virtual CefString GetPath() OVERRIDE {
    return plugin_info_.path.value();
  }

  virtual CefString GetVersion() OVERRIDE {
    return plugin_info_.version;
  }

  virtual CefString GetDescription() OVERRIDE {
    return plugin_info_.desc;
  }

 private:
  webkit::WebPluginInfo plugin_info_;

  IMPLEMENT_REFCOUNTING(CefWebPluginInfoImpl);
};

}  // namespace

size_t CefGetWebPluginCount() {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return 0;
  }

  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  std::vector<webkit::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPlugins(&plugins);
  return plugins.size();
}

CefRefPtr<CefWebPluginInfo> CefGetWebPluginInfo(int index) {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return 0;
  }

  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  std::vector<webkit::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPlugins(&plugins);

  if (index < 0 || index >= static_cast<int>(plugins.size()))
    return NULL;

  return new CefWebPluginInfoImpl(plugins[index]);
}

CefRefPtr<CefWebPluginInfo> CefGetWebPluginInfo(const CefString& name) {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return 0;
  }

  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  std::vector<webkit::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPlugins(&plugins);

  std::string nameStr = name;
  StringToLowerASCII(&nameStr);

  std::vector<webkit::WebPluginInfo>::const_iterator it = plugins.begin();
  for (; it != plugins.end(); ++it) {
    if (LowerCaseEqualsASCII(it->name, nameStr.c_str()))
      return new CefWebPluginInfoImpl(*it);
  }

  return NULL;
}
