// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: Need mechanism to cleanup the static instance

#ifndef _BROWSER_PLUGIN_LIST_H
#define _BROWSER_PLUGIN_LIST_H

#include <string>
#include <vector>

#include "../../include/cef_nplugin.h"

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "webkit/glue/webplugin.h"

class GURL;

namespace NPAPI
{

class BrowserPluginLib;
class PluginInstance;

// The BrowserPluginList is responsible for loading internal NPAPI based
// plugins. Call the LoadPlugin() method to load an internal plugin.
class BrowserPluginList : public base::RefCounted<BrowserPluginList> {
 public:
  // Gets the one instance of the BrowserPluginList.
  static BrowserPluginList* Singleton();

  virtual ~BrowserPluginList();

  // Add a plugin using the specified info structure.
  void AddPlugin(const struct CefPluginInfo& plugin_info);

  // Remove the plugin matching the specified info structure.
  void RemovePlugin(const struct CefPluginInfo& plugin_info);

  // Find a plugin by mime type and clsid.
  // If clsid is empty, we will just find the plugin that supports mime type.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  BrowserPluginLib* FindPlugin(const std::string &mime_type,
                               const std::string& clsid, bool allow_wildcard);

  // Find a plugin to by extension. Returns the corresponding mime type.
  BrowserPluginLib* FindPlugin(const GURL &url, std::string* actual_mime_type);

  // Check if we have any plugin for a given type.
  // mime_type must be all lowercase.
  bool SupportsType(const std::string &mime_type);

  // Returns true if the given WebPluginInfo supports a given file extension.
  // extension should be all lower case.
  // If mime_type is not NULL, it will be set to the mime type if found.
  // The mime type which corresponds to the extension is optionally returned
  // back.
  static bool SupportsExtension(const WebPluginInfo& info,
                                const std::string &extension,
                                std::string* actual_mime_type);

  // Shutdown all plugins.  Should be called at process teardown.
  void Shutdown();

  // Get all the plugins
  bool GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Returns true if a plugin is found for the given url and mime type.
  // The mime type which corresponds to the URL is optionally returned
  // back.
  // The allow_wildcard parameter controls whether this function returns
  // plugins which support wildcard mime types (* as the mime type)
  bool GetPluginInfo(const GURL& url,
                     const std::string& mime_type,
                     const std::string& clsid,
                     bool allow_wildcard,
                     struct CefPluginInfo* plugin_info,
                     std::string* actual_mime_type);
  
 private:
  // Constructors are private for singletons
  BrowserPluginList();

  static scoped_refptr<BrowserPluginList> singleton_;
  
  typedef std::vector<scoped_refptr<BrowserPluginLib> > PluginList;
  PluginList plugins_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserPluginList);
};

} // namespace NPAPI

#endif  // _BROWSER_PLUGIN_LIST_H

