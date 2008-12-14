// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_PLUGIN_LIB_H
#define _BROWSER_PLUGIN_LIB_H

#include <string>

#include "../../include/cef_nplugin.h"

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

struct WebPluginInfo;

namespace NPAPI
{

class BrowserPluginInstance;

// A BrowserPluginLib is a single NPAPI Plugin Library, and is the lifecycle
// manager for new PluginInstances.
class BrowserPluginLib : public base::RefCounted<BrowserPluginLib> {
 public:
  virtual ~BrowserPluginLib();
  static BrowserPluginLib* GetOrCreatePluginLib(const struct CefPluginInfo& plugin_info);

  // Get the Plugin's function pointer table.
  NPPluginFuncs *functions();

  // Returns true if this Plugin supports a given mime-type.
  // mime_type should be all lower case.
  bool SupportsType(const std::string &mime_type, bool allow_wildcard);

  // Creates a new instance of this plugin.
  BrowserPluginInstance *CreateInstance(const std::string &mime_type);

  // Called by the instance when the instance is tearing down.
  void CloseInstance();

  // Gets information about this plugin and the mime types that it
  // supports.
  const WebPluginInfo& web_plugin_info() { return *web_plugin_info_; }
  const struct CefPluginInfo& plugin_info() { return plugin_info_; }

  //
  // NPAPI functions
  //

  // NPAPI method to initialize a Plugin.
  // Initialize can be safely called multiple times
  NPError NP_Initialize();

  // NPAPI method to shutdown a Plugin.
  void NP_Shutdown(void);

  int instance_count() const { return instance_count_; }

 private:
  // Creates a new BrowserPluginLib.  The WebPluginInfo object is owned by this
  // object. internal_plugin_info must not be NULL.
  BrowserPluginLib(WebPluginInfo* info,
                   const CefPluginInfo& plugin_info);

  // Creates WebPluginInfo structure based on read in or built in
  // CefPluginVersionInfo.
  static WebPluginInfo* CreateWebPluginInfo(const CefPluginVersionInfo& info);

  scoped_ptr<WebPluginInfo> web_plugin_info_;  // supported mime types, description
  struct CefPluginInfo plugin_info_;
  NPPluginFuncs    plugin_funcs_;     // the struct of plugin side functions
  bool             initialized_;      // is the plugin initialized
  NPSavedData     *saved_data_;       // persisted plugin info for NPAPI
  int              instance_count_;   // count of plugins in use

  // A map of all the insantiated plugins.
  typedef base::hash_map<std::wstring, scoped_refptr<BrowserPluginLib> > PluginMap;
  static PluginMap* loaded_libs_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserPluginLib);
};

} // namespace NPAPI

#endif  // _BROWSER_PLUGIN_LIB_H
