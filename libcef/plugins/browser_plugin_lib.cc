// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"

#include "config.h"

#include "browser_plugin_lib.h"
#include "browser_plugin_instance.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_host.h"
#include "net/base/mime_util.h"


namespace NPAPI
{

BrowserPluginLib::PluginMap* BrowserPluginLib::loaded_libs_;

BrowserPluginLib* BrowserPluginLib::GetOrCreatePluginLib(const struct CefPluginInfo& plugin_info) {
  // We can only have one BrowserPluginLib object per plugin as it controls the
  // per instance function calls (i.e. NP_Initialize and NP_Shutdown).  So we
  // keep a (non-ref counted) map of BrowserPluginLib objects.
  if (!loaded_libs_)
    loaded_libs_ = new PluginMap();

  PluginMap::const_iterator iter =
      loaded_libs_->find(plugin_info.version_info.unique_name);
  if (iter != loaded_libs_->end())
    return iter->second;

  WebPluginInfo* info = CreateWebPluginInfo(plugin_info.version_info);
  return new BrowserPluginLib(info, plugin_info);
}

BrowserPluginLib::BrowserPluginLib(WebPluginInfo* info,
                                   const CefPluginInfo& plugin_info)
    : web_plugin_info_(info),
      plugin_info_(plugin_info),
      initialized_(false),
      saved_data_(0),
      instance_count_(0) {
   memset((void*)&plugin_funcs_, 0, sizeof(plugin_funcs_));

  (*loaded_libs_)[info->path.BaseName().value()] = this;
}

BrowserPluginLib::~BrowserPluginLib() {
  if (saved_data_ != 0) {
    // TODO - delete the savedData object here
  }
}

NPPluginFuncs *BrowserPluginLib::functions() {
  return &plugin_funcs_;
}

bool BrowserPluginLib::SupportsType(const std::string &mime_type,
                                    bool allow_wildcard) {
  // Webkit will ask for a plugin to handle empty mime types.
  if (mime_type.empty())
    return false;

  for (size_t i = 0; i < web_plugin_info_->mime_types.size(); ++i) {
    const WebPluginMimeType& mime_info = web_plugin_info_->mime_types[i];
    if (net::MatchesMimeType(mime_info.mime_type, mime_type)) {
      if (!allow_wildcard && (mime_info.mime_type == "*")) {
        continue;
      }
      return true;
    }
  }
  return false;
}

NPError BrowserPluginLib::NP_Initialize() {
  if (initialized_)
    return NPERR_NO_ERROR;

  PluginHost *host = PluginHost::Singleton();
  if (host == 0)
    return NPERR_GENERIC_ERROR;

  NPError rv = plugin_info_.np_initialize(host->host_functions());
  initialized_ = (rv == NPERR_NO_ERROR);

  if(initialized_) {
    plugin_funcs_.size = sizeof(plugin_funcs_);
    plugin_funcs_.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    if (plugin_info_.np_getentrypoints(&plugin_funcs_) != NPERR_NO_ERROR)
      rv = false;
  }

  return rv;
}

void BrowserPluginLib::NP_Shutdown(void) {
  DCHECK(initialized_);
  plugin_info_.np_shutdown();
}

BrowserPluginInstance *BrowserPluginLib::CreateInstance(const std::string &mime_type) {
  // The PluginInstance class uses the PluginLib member only for calling
  // CloseInstance() from the PluginInstance destructor.  We explicitly call
  // CloseInstance() from BrowserWebPluginDelegateImpl::DestroyInstance().
  BrowserPluginInstance *new_instance =
      new BrowserPluginInstance(this, mime_type);
  instance_count_++;
  DCHECK(new_instance != 0);
  return new_instance;
}

void BrowserPluginLib::CloseInstance() {
  instance_count_--;
  if (instance_count_ == 0) {
    NP_Shutdown();
    initialized_ = false;
    loaded_libs_->erase(web_plugin_info_->path.BaseName().value());
    if (loaded_libs_->empty()) {
      delete loaded_libs_;
      loaded_libs_ = NULL;
    }
  }
}

WebPluginInfo* BrowserPluginLib::CreateWebPluginInfo(const CefPluginVersionInfo& plugin_info) {
  std::vector<std::string> mime_types, file_extensions;
  std::vector<std::wstring> descriptions;
  SplitString(base::SysWideToNativeMB(plugin_info.mime_types), '|',
      &mime_types);
  SplitString(base::SysWideToNativeMB(plugin_info.file_extensions), '|',
      &file_extensions);
  SplitString(plugin_info.file_open_names, '|', &descriptions);

  if (mime_types.empty())
    return NULL;

  WebPluginInfo *info = new WebPluginInfo();
  info->name = plugin_info.product_name;
  info->desc = plugin_info.description;
  info->version = plugin_info.version;
  info->path = FilePath(plugin_info.unique_name);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    WebPluginMimeType mime_type;
    mime_type.mime_type = StringToLowerASCII(mime_types[i]);
    if (file_extensions.size() > i)
      SplitString(file_extensions[i], ',', &mime_type.file_extensions);

    if (descriptions.size() > i) {
      mime_type.description = descriptions[i];

      // Remove the extension list from the description.
      size_t ext = mime_type.description.find(L"(*");
      if (ext != std::wstring::npos) {
        if (ext > 1 && mime_type.description[ext -1] == ' ')
          ext--;

        mime_type.description.erase(ext);
      }
    }

    info->mime_types.push_back(mime_type);
  }

  return info;
}

}  // namespace NPAPI
