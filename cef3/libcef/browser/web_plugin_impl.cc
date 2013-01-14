// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/web_plugin_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "content/browser/plugin_service_impl.h"

namespace {

void PluginsCallbackImpl(
    CefRefPtr<CefWebPluginInfoVisitor> visitor,
    const std::vector<webkit::WebPluginInfo>& all_plugins) {
  CEF_REQUIRE_UIT();

  int count = 0;
  int total = static_cast<int>(all_plugins.size());

  std::vector<webkit::WebPluginInfo>::const_iterator it = all_plugins.begin();
  for (; it != all_plugins.end(); ++it, ++count) {
    CefRefPtr<CefWebPluginInfoImpl> info(new CefWebPluginInfoImpl(*it));
    if (!visitor->Visit(info.get(), count, total))
      break;
  }
}

}  // namespace


// CefWebPluginInfoImpl

CefWebPluginInfoImpl::CefWebPluginInfoImpl(
    const webkit::WebPluginInfo& plugin_info)
    : plugin_info_(plugin_info) {
}

CefString CefWebPluginInfoImpl::GetName() {
  return plugin_info_.name;
}

CefString CefWebPluginInfoImpl::GetPath() {
  return plugin_info_.path.value();
}

CefString CefWebPluginInfoImpl::GetVersion() {
  return plugin_info_.version;
}

CefString CefWebPluginInfoImpl::GetDescription() {
  return plugin_info_.desc;
}


// Global functions.

void CefVisitWebPluginInfo(CefRefPtr<CefWebPluginInfoVisitor> visitor) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (!visitor.get()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    content::PluginServiceImpl::GetInstance()->GetPlugins(
        base::Bind(PluginsCallbackImpl, visitor));
  } else {
    // Execute on the UI thread.
    CEF_POST_TASK(CEF_UIT, base::Bind(CefVisitWebPluginInfo, visitor));
  }
}

void CefRefreshWebPlugins() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->RefreshPlugins();
}

void CefAddWebPluginPath(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->AddExtraPluginPath(FilePath(path));
}

void CefAddWebPluginDirectory(const CefString& dir) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (dir.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->AddExtraPluginDir(FilePath(dir));
}

void CefRemoveWebPluginPath(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->RemoveExtraPluginPath(
      FilePath(path));
}

void CefUnregisterInternalWebPlugin(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  // No thread affinity.
  content::PluginServiceImpl::GetInstance()->UnregisterInternalPlugin(
      FilePath(path));
}

void CefForceWebPluginShutdown(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    content::PluginServiceImpl::GetInstance()->ForcePluginShutdown(
        FilePath(path));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CefForceWebPluginShutdown, path));
  }
}

void CefRegisterWebPluginCrash(const CefString& path) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    content::PluginServiceImpl::GetInstance()->RegisterPluginCrash(
        FilePath(path));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CefRegisterWebPluginCrash, path));
  }
}

void CefIsWebPluginUnstable(
    const CefString& path,
    CefRefPtr<CefWebPluginUnstableCallback> callback) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  if (path.empty() || !callback.get()) {
    NOTREACHED() << "invalid parameter";
    return;
  }

  if (CEF_CURRENTLY_ON_IOT()) {
    callback->IsUnstable(path,
        content::PluginServiceImpl::GetInstance()->IsPluginUnstable(
            FilePath(path)));
  } else {
    // Execute on the IO thread.
    CEF_POST_TASK(CEF_IOT, base::Bind(CefIsWebPluginUnstable, path, callback));
  }
}
