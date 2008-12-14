// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"

#include <algorithm>
#include <tchar.h>

#include "browser_plugin_list.h"
#include "browser_plugin_lib.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"


namespace NPAPI
{

scoped_refptr<BrowserPluginList> BrowserPluginList::singleton_;

BrowserPluginList* BrowserPluginList::Singleton() {
  if (singleton_.get() == NULL) {
    singleton_ = new BrowserPluginList();
  }

  return singleton_;
}

BrowserPluginList::BrowserPluginList() {
}

BrowserPluginList::~BrowserPluginList() {
  plugins_.clear();
}

void BrowserPluginList::AddPlugin(const struct CefPluginInfo& plugin_info) {
  scoped_refptr<BrowserPluginLib> new_plugin
      = BrowserPluginLib::GetOrCreatePluginLib(plugin_info);
  if (!new_plugin.get())
    return;

  const WebPluginInfo& web_plugin_info = new_plugin->web_plugin_info();
  for (size_t i = 0; i < web_plugin_info.mime_types.size(); ++i) {
    const std::string &mime_type = web_plugin_info.mime_types[i].mime_type;
    if (mime_type == "*")
      continue;

    if (!SupportsType(mime_type))
      plugins_.push_back(new_plugin);
  }
}

void BrowserPluginList::RemovePlugin(const struct CefPluginInfo& plugin_info)
{
  PluginList::iterator it = plugins_.begin();
  for(; it != plugins_.end(); ++it) {
    if((*it)->web_plugin_info().file == plugin_info.version_info.unique_name) {
      plugins_.erase(it);
    }
  }
}

BrowserPluginLib* BrowserPluginList::FindPlugin(const std::string& mime_type,
                                                const std::string& clsid,
                                                bool allow_wildcard) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));

  for (size_t idx = 0; idx < plugins_.size(); ++idx) {
    if (plugins_[idx]->SupportsType(mime_type, allow_wildcard)) {
      return plugins_[idx];
    }
  }

  return NULL;
}

BrowserPluginLib* BrowserPluginList::FindPlugin(const GURL &url,
                                                std::string* actual_mime_type) {
  std::wstring path = base::SysNativeMBToWide(url.path());
  std::wstring extension_wide = file_util::GetFileExtensionFromPath(path);
  if (extension_wide.empty())
    return NULL;;

  std::string extension =
      StringToLowerASCII(base::SysWideToNativeMB(extension_wide));

  for (size_t idx = 0; idx < plugins_.size(); ++idx) {
    if (SupportsExtension(plugins_[idx]->web_plugin_info(), extension,
        actual_mime_type)) {
      return plugins_[idx];
    }
  }

  return NULL;
}

bool BrowserPluginList::SupportsType(const std::string &mime_type) {
  DCHECK(mime_type == StringToLowerASCII(mime_type));
  bool allow_wildcard = true;
  return (FindPlugin(mime_type, "", allow_wildcard ) != 0);
}

bool BrowserPluginList::SupportsExtension(const WebPluginInfo& info,
                                          const std::string &extension,
                                          std::string* actual_mime_type) {
  for (size_t i = 0; i < info.mime_types.size(); ++i) {
    const WebPluginMimeType& mime_type = info.mime_types[i];
    for (size_t j = 0; j < mime_type.file_extensions.size(); ++j) {
      if (mime_type.file_extensions[j] == extension) {
        if (actual_mime_type)
          *actual_mime_type = mime_type.mime_type;
        return true;
      }
    }
  }

  return false;
}

bool BrowserPluginList::GetPlugins(bool refresh,
                                   std::vector<WebPluginInfo>* plugins) {
  plugins->resize(plugins_.size());
  for (size_t i = 0; i < plugins->size(); ++i)
    (*plugins)[i] = plugins_[i]->web_plugin_info();

  return true;
}

bool BrowserPluginList::GetPluginInfo(const GURL& url,
                                      const std::string& mime_type,
                                      const std::string& clsid,
                                      bool allow_wildcard,
                                      struct CefPluginInfo* plugin_info,
                                      std::string* actual_mime_type) {
  scoped_refptr<BrowserPluginLib> plugin = FindPlugin(mime_type, clsid,
                                                      allow_wildcard);

  if (plugin.get() == NULL)
    plugin = FindPlugin(url, actual_mime_type);

  if (plugin.get() == NULL)
    return false;

  *plugin_info = plugin->plugin_info();
  return true;
}

void BrowserPluginList::Shutdown() {
  // TODO
}

} // namespace NPAPI
