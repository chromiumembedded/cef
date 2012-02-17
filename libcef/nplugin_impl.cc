// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_nplugin.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

void UIT_RegisterPlugin(CefPluginInfo* plugin_info) {
  REQUIRE_UIT();

  webkit::WebPluginInfo info;

  info.path = FilePath(CefString(&plugin_info->unique_name));
  info.name = CefString(&plugin_info->display_name);
  info.version = CefString(&plugin_info->version);
  info.desc = CefString(&plugin_info->description);

  typedef std::vector<std::string> VectorType;
  VectorType mime_types, file_extensions, descriptions, file_extensions_parts;
  base::SplitString(CefString(&plugin_info->mime_types), '|', &mime_types);
  base::SplitString(CefString(&plugin_info->file_extensions), '|',
      &file_extensions);
  base::SplitString(CefString(&plugin_info->type_descriptions), '|',
      &descriptions);

  for (size_t i = 0; i < mime_types.size(); ++i) {
    webkit::WebPluginMimeType mimeType;

    mimeType.mime_type = mime_types[i];

    if (file_extensions.size() > i) {
      base::SplitString(file_extensions[i], ',', &file_extensions_parts);
      VectorType::const_iterator it = file_extensions_parts.begin();
      for (; it != file_extensions_parts.end(); ++it)
        mimeType.file_extensions.push_back(*(it));
      file_extensions_parts.clear();
    }

    if (descriptions.size() > i)
      mimeType.description = UTF8ToUTF16(descriptions[i]);

    info.mime_types.push_back(mimeType);
  }

  webkit::npapi::PluginEntryPoints entry_points;
#if !defined(OS_POSIX) || defined(OS_MACOSX)
  entry_points.np_getentrypoints = plugin_info->np_getentrypoints;
#endif
  entry_points.np_initialize = plugin_info->np_initialize;
  entry_points.np_shutdown = plugin_info->np_shutdown;

  webkit::npapi::PluginList::Singleton()->RegisterInternalPluginWithEntryPoints(
      info, true, entry_points);

  delete plugin_info;
}

}  // namespace

bool CefRegisterPlugin(const CefPluginInfo& plugin_info) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  CefThread::PostTask(CefThread::UI, FROM_HERE,
      base::Bind(UIT_RegisterPlugin, new CefPluginInfo(plugin_info)));

  return true;
}
