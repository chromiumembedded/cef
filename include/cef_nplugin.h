// Copyright (c) 2008 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef _CEF_PLUGIN_H
#define _CEF_PLUGIN_H

#include <string>
#include "webkit/glue/plugins/nphostapi.h"
#include "third_party/npapi/bindings/npapi.h"

// Netscape plugins are normally built at separate DLLs that are loaded by the
// browser when needed.  This interface supports the creation of plugins that
// are an embedded component of the application.  Embedded plugins built using
// this interface use the same Netscape Plugin API as DLL-based plugins.
// See https://developer.mozilla.org/En/Gecko_Plugin_API_Reference for complete
// documentation on how to use the Netscape Plugin API.

// This structure fully describes a plugin.
struct CefPluginVersionInfo {
  // Unique name used to identify a plugin.  The unique name is used in place
  // of the file path that would be available with normal plugin DLLs.
  std::wstring unique_name;
  std::wstring product_name;
  std::wstring description;
  std::wstring version;
  // List of supported mime type values, delimited with a pipe (|) character.
  std::wstring mime_types;
  // List of supported file extensions, delimited with a pipe (|) character.
  std::wstring file_extensions;
  // List of descriptions for the file extensions, delimited with a pipe (|)
  // character.
  std::wstring file_open_names;
};

// This structure provides version information and entry point functions.
struct CefPluginInfo {
  CefPluginVersionInfo version_info;
  NP_GetEntryPointsFunc np_getentrypoints;
  NP_InitializeFunc np_initialize;
  NP_ShutdownFunc np_shutdown;
};

// Register the plugin with the system.
bool CefRegisterPlugin(const struct CefPluginInfo& plugin_info);

// Unregister the plugin with the system.
bool CefUnregisterPlugin(const struct CefPluginInfo& plugin_info);

#endif // _CEF_PLUGIN_H
